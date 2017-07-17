/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

#include "../gl_driver.h"
#include "common/common.h"
#include "serialise/string_utils.h"

bool WrappedOpenGL::Serialise_glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y,
                                                GLuint num_groups_z)
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
    AddEvent(desc);
    string name =
        "glDispatchCompute(" + ToStr::Get(X) + ", " + ToStr::Get(Y) + ", " + ToStr::Get(Z) + ")";

    DrawcallDescription draw;
    draw.name = name;
    draw.flags |= DrawFlags::Dispatch;

    draw.dispatchDimension[0] = X;
    draw.dispatchDimension[1] = Y;
    draw.dispatchDimension[2] = Z;

    if(X == 0)
      AddDebugMessage(MessageCategory::Execution, MessageSeverity::Medium,
                      MessageSource::IncorrectAPIUse,
                      "Dispatch call has Num Groups X=0. This will do nothing, which is unusual "
                      "for a non-indirect Dispatch. Did you mean X=1?");
    if(Y == 0)
      AddDebugMessage(MessageCategory::Execution, MessageSeverity::Medium,
                      MessageSource::IncorrectAPIUse,
                      "Dispatch call has Num Groups Y=0. This will do nothing, which is unusual "
                      "for a non-indirect Dispatch. Did you mean Y=1?");
    if(Z == 0)
      AddDebugMessage(MessageCategory::Execution, MessageSeverity::Medium,
                      MessageSource::IncorrectAPIUse,
                      "Dispatch call has Num Groups Z=0. This will do nothing, which is unusual "
                      "for a non-indirect Dispatch. Did you mean Z=1?");

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

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
  }
}

bool WrappedOpenGL::Serialise_glDispatchComputeGroupSizeARB(GLuint num_groups_x, GLuint num_groups_y,
                                                            GLuint num_groups_z, GLuint group_size_x,
                                                            GLuint group_size_y, GLuint group_size_z)
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
    AddEvent(desc);
    string name = "glDispatchComputeGroupSizeARB(" + ToStr::Get(X) + ", " + ToStr::Get(Y) + ", " +
                  ToStr::Get(Z) + ", " + ToStr::Get(sX) + ", " + ToStr::Get(sY) + ", " +
                  ToStr::Get(sZ) + ")";

    DrawcallDescription draw;
    draw.name = name;
    draw.flags |= DrawFlags::Dispatch;

    draw.dispatchDimension[0] = X;
    draw.dispatchDimension[1] = Y;
    draw.dispatchDimension[2] = Z;
    draw.dispatchThreadsDimension[0] = sX;
    draw.dispatchThreadsDimension[1] = sY;
    draw.dispatchThreadsDimension[2] = sZ;

    if(X == 0)
      AddDebugMessage(MessageCategory::Execution, MessageSeverity::Medium,
                      MessageSource::IncorrectAPIUse,
                      "Dispatch call has Num Groups X=0. This will do nothing, which is unusual "
                      "for a non-indirect Dispatch. Did you mean X=1?");
    if(Y == 0)
      AddDebugMessage(MessageCategory::Execution, MessageSeverity::Medium,
                      MessageSource::IncorrectAPIUse,
                      "Dispatch call has Num Groups Y=0. This will do nothing, which is unusual "
                      "for a non-indirect Dispatch. Did you mean Y=1?");
    if(Z == 0)
      AddDebugMessage(MessageCategory::Execution, MessageSeverity::Medium,
                      MessageSource::IncorrectAPIUse,
                      "Dispatch call has Num Groups Z=0. This will do nothing, which is unusual "
                      "for a non-indirect Dispatch. Did you mean Z=1?");

    if(sX == 0)
      AddDebugMessage(MessageCategory::Execution, MessageSeverity::Medium,
                      MessageSource::IncorrectAPIUse,
                      "Dispatch call has Group Size X=0. This will do nothing, which is unusual "
                      "for a non-indirect Dispatch. Did you mean X=1?");
    if(sY == 0)
      AddDebugMessage(MessageCategory::Execution, MessageSeverity::Medium,
                      MessageSource::IncorrectAPIUse,
                      "Dispatch call has Group Size Y=0. This will do nothing, which is unusual "
                      "for a non-indirect Dispatch. Did you mean Y=1?");
    if(sZ == 0)
      AddDebugMessage(MessageCategory::Execution, MessageSeverity::Medium,
                      MessageSource::IncorrectAPIUse,
                      "Dispatch call has Group Size Z=0. This will do nothing, which is unusual "
                      "for a non-indirect Dispatch. Did you mean Z=1?");

    AddDrawcall(draw, true);
  }

  return true;
}

void WrappedOpenGL::glDispatchComputeGroupSizeARB(GLuint num_groups_x, GLuint num_groups_y,
                                                  GLuint num_groups_z, GLuint group_size_x,
                                                  GLuint group_size_y, GLuint group_size_z)
{
  CoherentMapImplicitBarrier();

  m_Real.glDispatchComputeGroupSizeARB(num_groups_x, num_groups_y, num_groups_z, group_size_x,
                                       group_size_y, group_size_z);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(DISPATCH_COMPUTE_GROUP_SIZE);
    Serialise_glDispatchComputeGroupSizeARB(num_groups_x, num_groups_y, num_groups_z, group_size_x,
                                            group_size_y, group_size_z);

    m_ContextRecord->AddChunk(scope.Get());

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
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
    m_Real.glGetBufferSubData(eGL_DISPATCH_INDIRECT_BUFFER, (GLintptr)offs, sizeof(uint32_t) * 3,
                              groupSizes);

    AddEvent(desc);
    string name = "glDispatchComputeIndirect(<" + ToStr::Get(groupSizes[0]) + ", " +
                  ToStr::Get(groupSizes[1]) + ", " + ToStr::Get(groupSizes[2]) + ">)";

    DrawcallDescription draw;
    draw.name = name;
    draw.flags |= DrawFlags::Dispatch | DrawFlags::Indirect;

    draw.dispatchDimension[0] = groupSizes[0];
    draw.dispatchDimension[1] = groupSizes[1];
    draw.dispatchDimension[2] = groupSizes[2];

    AddDrawcall(draw, true);

    GLuint buf = 0;
    m_Real.glGetIntegerv(eGL_DISPATCH_INDIRECT_BUFFER_BINDING, (GLint *)&buf);

    m_ResourceUses[GetResourceManager()->GetID(BufferRes(GetCtx(), buf))].push_back(
        EventUsage(m_CurEventID, ResourceUsage::Indirect));
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

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
  }
}

enum MemoryBarrierBitfield
{
};

template <>
string ToStrHelper<false, MemoryBarrierBitfield>::Get(const MemoryBarrierBitfield &el)
{
  string ret;

  if(el == (MemoryBarrierBitfield)GL_ALL_BARRIER_BITS)
    return "GL_ALL_BARRIER_BITS";

  if(el & GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT)
    ret += " | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT";
  if(el & GL_ELEMENT_ARRAY_BARRIER_BIT)
    ret += " | GL_ELEMENT_ARRAY_BARRIER_BIT";
  if(el & GL_UNIFORM_BARRIER_BIT)
    ret += " | GL_UNIFORM_BARRIER_BIT";
  if(el & GL_TEXTURE_FETCH_BARRIER_BIT)
    ret += " | GL_TEXTURE_FETCH_BARRIER_BIT";
  if(el & GL_SHADER_IMAGE_ACCESS_BARRIER_BIT)
    ret += " | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT";
  if(el & GL_COMMAND_BARRIER_BIT)
    ret += " | GL_COMMAND_BARRIER_BIT";
  if(el & GL_PIXEL_BUFFER_BARRIER_BIT)
    ret += " | GL_PIXEL_BUFFER_BARRIER_BIT";
  if(el & GL_TEXTURE_UPDATE_BARRIER_BIT)
    ret += " | GL_TEXTURE_UPDATE_BARRIER_BIT";
  if(el & GL_BUFFER_UPDATE_BARRIER_BIT)
    ret += " | GL_BUFFER_UPDATE_BARRIER_BIT";
  if(el & GL_FRAMEBUFFER_BARRIER_BIT)
    ret += " | GL_FRAMEBUFFER_BARRIER_BIT";
  if(el & GL_TRANSFORM_FEEDBACK_BARRIER_BIT)
    ret += " | GL_TRANSFORM_FEEDBACK_BARRIER_BIT";
  if(el & GL_ATOMIC_COUNTER_BARRIER_BIT)
    ret += " | GL_ATOMIC_COUNTER_BARRIER_BIT";
  if(el & GL_SHADER_STORAGE_BARRIER_BIT)
    ret += " | GL_SHADER_STORAGE_BARRIER_BIT";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

bool WrappedOpenGL::Serialise_glMemoryBarrier(GLbitfield barriers)
{
  MemoryBarrierBitfield b = (MemoryBarrierBitfield)barriers;
  SERIALISE_ELEMENT(MemoryBarrierBitfield, Barriers, b);
  RDCCOMPILE_ASSERT(sizeof(MemoryBarrierBitfield) == sizeof(uint32_t),
                    "Fake bitfield enum must be uint32_t sized");

  if(m_State <= EXECUTING)
  {
    m_Real.glMemoryBarrier((GLbitfield)Barriers);
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
  MemoryBarrierBitfield b = (MemoryBarrierBitfield)barriers;
  SERIALISE_ELEMENT(MemoryBarrierBitfield, Barriers, b);
  RDCCOMPILE_ASSERT(sizeof(MemoryBarrierBitfield) == sizeof(uint32_t),
                    "Fake bitfield enum must be uint32_t sized");

  if(m_State <= EXECUTING)
  {
    m_Real.glMemoryBarrierByRegion((GLbitfield)Barriers);
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
    m_Real.glDrawTransformFeedback(
        Mode, fid == ResourceId() ? 0 : GetResourceManager()->GetLiveResource(fid).name);
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);
    string name = "glDrawTransformFeedback(<?>)";

    GLNOTIMP("Not fetching feedback object count for glDrawTransformFeedback() display");

    DrawcallDescription draw;
    draw.name = name;
    draw.numIndices = 1;
    draw.numInstances = 1;
    draw.indexOffset = 0;
    draw.vertexOffset = 0;
    draw.instanceOffset = 0;

    draw.flags |= DrawFlags::Drawcall;

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

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
  }
}

bool WrappedOpenGL::Serialise_glDrawTransformFeedbackInstanced(GLenum mode, GLuint id,
                                                               GLsizei instancecount)
{
  SERIALISE_ELEMENT(GLenum, Mode, mode);
  SERIALISE_ELEMENT(ResourceId, fid, GetResourceManager()->GetID(FeedbackRes(GetCtx(), id)));
  SERIALISE_ELEMENT(uint32_t, Count, instancecount);

  if(m_State <= EXECUTING)
  {
    m_Real.glDrawTransformFeedbackInstanced(
        Mode, fid == ResourceId() ? 0 : GetResourceManager()->GetLiveResource(fid).name, Count);
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);
    string name = "glDrawTransformFeedbackInstanced(<?>)";

    GLNOTIMP("Not fetching feedback object count for glDrawTransformFeedbackInstanced() display");

    DrawcallDescription draw;
    draw.name = name;
    draw.numIndices = 1;
    draw.numInstances = 1;
    draw.indexOffset = 0;
    draw.vertexOffset = 0;
    draw.instanceOffset = 0;

    draw.flags |= DrawFlags::Drawcall;

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

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
  }
}

bool WrappedOpenGL::Serialise_glDrawTransformFeedbackStream(GLenum mode, GLuint id, GLuint stream)
{
  SERIALISE_ELEMENT(GLenum, Mode, mode);
  SERIALISE_ELEMENT(ResourceId, fid, GetResourceManager()->GetID(FeedbackRes(GetCtx(), id)));
  SERIALISE_ELEMENT(uint32_t, Stream, stream);

  if(m_State <= EXECUTING)
  {
    m_Real.glDrawTransformFeedbackStream(
        Mode, fid == ResourceId() ? 0 : GetResourceManager()->GetLiveResource(fid).name, Stream);
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);
    string name = "glDrawTransformFeedbackStream(<?>)";

    GLNOTIMP("Not fetching feedback object count for glDrawTransformFeedbackStream() display");

    DrawcallDescription draw;
    draw.name = name;
    draw.numIndices = 1;
    draw.numInstances = 1;
    draw.indexOffset = 0;
    draw.vertexOffset = 0;
    draw.instanceOffset = 0;

    draw.flags |= DrawFlags::Drawcall;

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

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
  }
}

bool WrappedOpenGL::Serialise_glDrawTransformFeedbackStreamInstanced(GLenum mode, GLuint id,
                                                                     GLuint stream,
                                                                     GLsizei instancecount)
{
  SERIALISE_ELEMENT(GLenum, Mode, mode);
  SERIALISE_ELEMENT(ResourceId, fid, GetResourceManager()->GetID(FeedbackRes(GetCtx(), id)));
  SERIALISE_ELEMENT(uint32_t, Stream, stream);
  SERIALISE_ELEMENT(uint32_t, Count, instancecount);

  if(m_State <= EXECUTING)
  {
    m_Real.glDrawTransformFeedbackStreamInstanced(
        Mode, fid == ResourceId() ? 0 : GetResourceManager()->GetLiveResource(fid).name, Stream,
        Count);
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);
    string name = "glDrawTransformFeedbackStreamInstanced(<?>)";

    GLNOTIMP(
        "Not fetching feedback object count for glDrawTransformFeedbackStreamInstanced() display");

    DrawcallDescription draw;
    draw.name = name;
    draw.numIndices = 1;
    draw.numInstances = 1;
    draw.indexOffset = 0;
    draw.vertexOffset = 0;
    draw.instanceOffset = 0;

    draw.flags |= DrawFlags::Drawcall;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);

    AddDrawcall(draw, true);
  }

  return true;
}

void WrappedOpenGL::glDrawTransformFeedbackStreamInstanced(GLenum mode, GLuint id, GLuint stream,
                                                           GLsizei instancecount)
{
  CoherentMapImplicitBarrier();

  m_Real.glDrawTransformFeedbackStreamInstanced(mode, id, stream, instancecount);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(DRAW_FEEDBACK_STREAM_INSTANCED);
    Serialise_glDrawTransformFeedbackStreamInstanced(mode, id, stream, instancecount);

    m_ContextRecord->AddChunk(scope.Get());

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
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
    AddEvent(desc);
    string name = "glDrawArrays(" + ToStr::Get(Count) + ")";

    DrawcallDescription draw;
    draw.name = name;
    draw.numIndices = Count;
    draw.numInstances = 1;
    draw.indexOffset = 0;
    draw.vertexOffset = First;
    draw.instanceOffset = 0;

    draw.flags |= DrawFlags::Drawcall;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);

    AddDrawcall(draw, true);
  }

  return true;
}

WrappedOpenGL::ClientMemoryData *WrappedOpenGL::CopyClientMemoryArrays(GLint first, GLsizei count,
                                                                       GLenum indexType,
                                                                       const void *&indices)
{
  RDCASSERT(m_State == WRITING_CAPFRAME);
  ContextData &cd = GetCtxData();

  GLint idxbuf = 0;
  GLsizeiptr idxlen = 0;
  const void *mmIndices = indices;
  if(indexType != eGL_NONE)
  {
    uint32_t IdxSize = indexType == eGL_UNSIGNED_BYTE ? 1 : indexType == eGL_UNSIGNED_SHORT
                                                                ? 2
                                                                : /*Type == eGL_UNSIGNED_INT*/ 4;
    idxlen = GLsizeiptr(IdxSize * count);

    m_Real.glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, &idxbuf);
    if(idxbuf == 0)
    {
      // Bind and update fake index buffer, to draw from the 'immediate' index data
      glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, cd.m_ClientMemoryIBO);
      glBufferData(eGL_ELEMENT_ARRAY_BUFFER, idxlen, indices, eGL_STATIC_DRAW);

      // Set offset to 0 - means we read data from start of our fake index buffer
      indices = 0;
    }
  }

  GLResourceRecord *varecord = cd.m_VertexArrayRecord;
  if(varecord)    // Early out if VAO bound, as VAOs are VBO-only.
    return NULL;

  ClientMemoryData *clientMemory = new ClientMemoryData;
  m_Real.glGetIntegerv(eGL_ARRAY_BUFFER_BINDING, (GLint *)&clientMemory->prevArrayBufferBinding);

  for(GLuint i = 0; i < ARRAY_COUNT(cd.m_ClientMemoryVBOs); i++)
  {
    GLint enabled = 0;
    m_Real.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
    if(!enabled)
      continue;

    // Check that the attrib is using client-memory.
    GLuint buffer;
    m_Real.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, (GLint *)&buffer);
    if(buffer != 0)
      continue;

    if(indexType != eGL_NONE && first == -1)
    {
      // First time we know we are using client-memory along with indices.
      // Iterate over the indices to find the range of client memory to copy.
      if(idxbuf != 0)
      {
        // If we were using a real index buffer, read it back to check its range.
        mmIndices = m_Real.glMapBufferRange(eGL_ELEMENT_ARRAY_BUFFER, (size_t)indices, idxlen,
                                            eGL_MAP_READ_BIT);
      }

      size_t min = ~0u, max = 0;
      GLsizei j;
      switch(indexType)
      {
        case eGL_UNSIGNED_BYTE:
          for(j = 0; j < count; j++)
          {
            min = RDCMIN(min, (size_t)((GLubyte *)mmIndices)[j]);
            max = RDCMAX(max, (size_t)((GLubyte *)mmIndices)[j]);
          }
          break;
        case eGL_UNSIGNED_SHORT:
          for(j = 0; j < count; j++)
          {
            min = RDCMIN(min, (size_t)((GLushort *)mmIndices)[j]);
            max = RDCMAX(max, (size_t)((GLushort *)mmIndices)[j]);
          }
          break;
        case eGL_UNSIGNED_INT:
          for(j = 0; j < count; j++)
          {
            min = RDCMIN(min, (size_t)((GLuint *)mmIndices)[j]);
            max = RDCMAX(max, (size_t)((GLuint *)mmIndices)[j]);
          }
          break;
        default:;
      }

      first = (GLint)min;
      count = (GLint)(max - min + 1);

      if(idxbuf != 0)
        m_Real.glUnmapBuffer(eGL_ELEMENT_ARRAY_BUFFER);
    }

    // App initially used client memory, so copy it into the temporary buffer.
    ClientMemoryData::VertexAttrib attrib;
    memset(&attrib, 0, sizeof(attrib));
    attrib.index = i;
    m_Real.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_SIZE, &attrib.size);
    m_Real.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_TYPE, (GLint *)&attrib.type);
    m_Real.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_NORMALIZED, (GLint *)&attrib.normalized);
    m_Real.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_STRIDE, &attrib.stride);
    m_Real.glGetVertexAttribPointerv(i, eGL_VERTEX_ATTRIB_ARRAY_POINTER, &attrib.pointer);

    GLint totalStride = attrib.stride ? attrib.stride : (GLint)GLTypeSize(attrib.type) * attrib.size;
    glBindBuffer(eGL_ARRAY_BUFFER, cd.m_ClientMemoryVBOs[i]);
    // Copy all client memory, and the pointer becomes a zero offset.
    glBufferData(eGL_ARRAY_BUFFER, (first + count) * totalStride, attrib.pointer, eGL_STATIC_DRAW);
    glVertexAttribPointer(attrib.index, attrib.size, attrib.type, attrib.normalized, attrib.stride,
                          NULL);

    clientMemory->attribs.push_back(attrib);
  }

  return clientMemory;
}

void WrappedOpenGL::RestoreClientMemoryArrays(ClientMemoryData *clientMemoryArrays, GLenum indexType)
{
  if(indexType != eGL_NONE)
  {
    ContextData &cd = GetCtxData();
    GLuint idxbuf = 0;
    m_Real.glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, (GLint *)&idxbuf);
    if(idxbuf == cd.m_ClientMemoryIBO)
      // Restore the zero buffer binding if we were using the fake index buffer.
      glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, 0);
  }

  if(!clientMemoryArrays)
    return;

  // Restore the 0-buffer bindings and attrib pointers.
  glBindBuffer(eGL_ARRAY_BUFFER, 0);
  for(const ClientMemoryData::VertexAttrib &attrib : clientMemoryArrays->attribs)
  {
    glVertexAttribPointer(attrib.index, attrib.size, attrib.type, attrib.normalized, attrib.stride,
                          attrib.pointer);
  }
  glBindBuffer(eGL_ARRAY_BUFFER, clientMemoryArrays->prevArrayBufferBinding);

  delete clientMemoryArrays;
}

void WrappedOpenGL::glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
  CoherentMapImplicitBarrier();

  m_Real.glDrawArrays(mode, first, count);

  if(m_State == WRITING_CAPFRAME)
  {
    const void *indices = NULL;
    ClientMemoryData *clientMemory = CopyClientMemoryArrays(first, count, eGL_NONE, indices);

    SCOPED_SERIALISE_CONTEXT(DRAWARRAYS);
    Serialise_glDrawArrays(mode, first, count);

    m_ContextRecord->AddChunk(scope.Get());

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);

    RestoreClientMemoryArrays(clientMemory, eGL_NONE);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
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

    AddEvent(desc);
    string name = "glDrawArraysIndirect(" + ToStr::Get(params.count) + ", " +
                  ToStr::Get(params.instanceCount) + ">)";

    DrawcallDescription draw;
    draw.name = name;
    draw.numIndices = params.count;
    draw.numInstances = params.instanceCount;
    draw.vertexOffset = params.first;
    draw.instanceOffset = params.baseInstance;

    draw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::Indirect;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);

    AddDrawcall(draw, true);

    GLuint buf = 0;
    m_Real.glGetIntegerv(eGL_DRAW_INDIRECT_BUFFER_BINDING, (GLint *)&buf);

    m_ResourceUses[GetResourceManager()->GetID(BufferRes(GetCtx(), buf))].push_back(
        EventUsage(m_CurEventID, ResourceUsage::Indirect));
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

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
  }
}

bool WrappedOpenGL::Serialise_glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count,
                                                    GLsizei instancecount)
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
    AddEvent(desc);
    string name =
        "glDrawArraysInstanced(" + ToStr::Get(Count) + ", " + ToStr::Get(InstanceCount) + ")";

    DrawcallDescription draw;
    draw.name = name;
    draw.numIndices = Count;
    draw.numInstances = InstanceCount;
    draw.indexOffset = 0;
    draw.vertexOffset = First;
    draw.instanceOffset = 0;

    draw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);

    AddDrawcall(draw, true);
  }

  return true;
}

void WrappedOpenGL::glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count,
                                          GLsizei instancecount)
{
  CoherentMapImplicitBarrier();

  m_Real.glDrawArraysInstanced(mode, first, count, instancecount);

  if(m_State == WRITING_CAPFRAME)
  {
    const void *indices = NULL;
    ClientMemoryData *clientMemory = CopyClientMemoryArrays(first, count, eGL_NONE, indices);

    SCOPED_SERIALISE_CONTEXT(DRAWARRAYS_INSTANCED);
    Serialise_glDrawArraysInstanced(mode, first, count, instancecount);

    m_ContextRecord->AddChunk(scope.Get());

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);

    RestoreClientMemoryArrays(clientMemory, eGL_NONE);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
  }
}

bool WrappedOpenGL::Serialise_glDrawArraysInstancedBaseInstance(GLenum mode, GLint first,
                                                                GLsizei count, GLsizei instancecount,
                                                                GLuint baseinstance)
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
    AddEvent(desc);
    string name = "glDrawArraysInstancedBaseInstance(" + ToStr::Get(Count) + ", " +
                  ToStr::Get(InstanceCount) + ")";

    DrawcallDescription draw;
    draw.name = name;
    draw.numIndices = Count;
    draw.numInstances = InstanceCount;
    draw.indexOffset = 0;
    draw.vertexOffset = First;
    draw.instanceOffset = BaseInstance;

    draw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);

    AddDrawcall(draw, true);
  }

  return true;
}

void WrappedOpenGL::glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count,
                                                      GLsizei instancecount, GLuint baseinstance)
{
  CoherentMapImplicitBarrier();

  m_Real.glDrawArraysInstancedBaseInstance(mode, first, count, instancecount, baseinstance);

  if(m_State == WRITING_CAPFRAME)
  {
    const void *indices = NULL;
    ClientMemoryData *clientMemory = CopyClientMemoryArrays(first, count, eGL_NONE, indices);

    SCOPED_SERIALISE_CONTEXT(DRAWARRAYS_INSTANCEDBASEINSTANCE);
    Serialise_glDrawArraysInstancedBaseInstance(mode, first, count, instancecount, baseinstance);

    m_ContextRecord->AddChunk(scope.Get());

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);

    RestoreClientMemoryArrays(clientMemory, eGL_NONE);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
  }
}

bool WrappedOpenGL::Check_preElements()
{
  GLint idxbuf = 0;
  m_Real.glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, &idxbuf);

  if(idxbuf == 0)
  {
    AddDebugMessage(MessageCategory::Undefined, MessageSeverity::High,
                    MessageSource::IncorrectAPIUse, "No index buffer bound at indexed draw!.");
    return false;
  }

  return true;
}

void WrappedOpenGL::Legacy_preElements(GLenum Type, uint32_t Count)
{
  if(m_State <= EXECUTING && GetLogVersion() <= 0x000015)
  {
    // in older logs there used to be a different way of manually saving client-side memory
    // indices.
    // We don't support replaying this anymore, but we need to match serialisation to be able to
    // open older captures - in 99% of cases the bool will be false. When it's true, we just add
    // an error message about it.
    SERIALISE_ELEMENT(bool, IndicesFromMemory, false);

    if(IndicesFromMemory)
    {
      uint32_t IdxSize = Type == eGL_UNSIGNED_BYTE ? 1 : Type == eGL_UNSIGNED_SHORT
                                                             ? 2
                                                             : /*Type == eGL_UNSIGNED_INT*/ 4;

      // serialise the data, even unused
      SERIALISE_ELEMENT_BUF(byte *, idxdata, NULL, size_t(IdxSize * Count));

      AddDebugMessage(MessageCategory::Deprecated, MessageSeverity::High,
                      MessageSource::UnsupportedConfiguration,
                      "Client-side index data used at drawcall, re-capture with a new version to "
                      "replay this draw.");

      SAFE_DELETE_ARRAY(idxdata);
    }
  }
}

bool WrappedOpenGL::Serialise_glDrawElements(GLenum mode, GLsizei count, GLenum type,
                                             const void *indices)
{
  SERIALISE_ELEMENT(GLenum, Mode, mode);
  SERIALISE_ELEMENT(uint32_t, Count, count);
  SERIALISE_ELEMENT(GLenum, Type, type);
  SERIALISE_ELEMENT(uint64_t, IdxOffset, (uint64_t)indices);

  if(m_State <= EXECUTING)
  {
    Legacy_preElements(Type, Count);

    if(Check_preElements())
      m_Real.glDrawElements(Mode, Count, Type, (const void *)IdxOffset);
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);
    string name = "glDrawElements(" + ToStr::Get(Count) + ")";

    uint32_t IdxSize = Type == eGL_UNSIGNED_BYTE ? 1 : Type == eGL_UNSIGNED_SHORT
                                                           ? 2
                                                           : /*Type == eGL_UNSIGNED_INT*/ 4;

    DrawcallDescription draw;
    draw.name = name;
    draw.numIndices = Count;
    draw.numInstances = 1;
    draw.indexOffset = uint32_t(IdxOffset) / IdxSize;
    draw.vertexOffset = 0;
    draw.instanceOffset = 0;

    draw.flags |= DrawFlags::Drawcall | DrawFlags::UseIBuffer;

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
    ClientMemoryData *clientMemory = CopyClientMemoryArrays(-1, count, type, indices);

    SCOPED_SERIALISE_CONTEXT(DRAWELEMENTS);
    Serialise_glDrawElements(mode, count, type, indices);

    m_ContextRecord->AddChunk(scope.Get());

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);

    RestoreClientMemoryArrays(clientMemory, type);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
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

    AddEvent(desc);
    string name = "glDrawElementsIndirect(" + ToStr::Get(params.count) + ", " +
                  ToStr::Get(params.instanceCount) + ">)";

    uint32_t IdxSize = Type == eGL_UNSIGNED_BYTE ? 1 : Type == eGL_UNSIGNED_SHORT
                                                           ? 2
                                                           : /*Type == eGL_UNSIGNED_INT*/ 4;

    DrawcallDescription draw;
    draw.name = name;
    draw.numIndices = params.count;
    draw.numInstances = params.instanceCount;
    draw.indexOffset = params.firstIndex;
    draw.baseVertex = params.baseVertex;
    draw.instanceOffset = params.baseInstance;

    draw.flags |=
        DrawFlags::Drawcall | DrawFlags::UseIBuffer | DrawFlags::Instanced | DrawFlags::Indirect;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);
    draw.indexByteWidth = IdxSize;

    AddDrawcall(draw, true);

    GLuint buf = 0;
    m_Real.glGetIntegerv(eGL_DRAW_INDIRECT_BUFFER_BINDING, (GLint *)&buf);

    m_ResourceUses[GetResourceManager()->GetID(BufferRes(GetCtx(), buf))].push_back(
        EventUsage(m_CurEventID, ResourceUsage::Indirect));
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

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
  }
}

bool WrappedOpenGL::Serialise_glDrawRangeElements(GLenum mode, GLuint start, GLuint end,
                                                  GLsizei count, GLenum type, const void *indices)
{
  SERIALISE_ELEMENT(GLenum, Mode, mode);
  SERIALISE_ELEMENT(uint32_t, Start, start);
  SERIALISE_ELEMENT(uint32_t, End, end);
  SERIALISE_ELEMENT(uint32_t, Count, count);
  SERIALISE_ELEMENT(GLenum, Type, type);
  SERIALISE_ELEMENT(uint64_t, IdxOffset, (uint64_t)indices);

  if(m_State <= EXECUTING)
  {
    Legacy_preElements(Type, Count);

    if(Check_preElements())
      m_Real.glDrawRangeElements(Mode, Start, End, Count, Type, (const void *)IdxOffset);
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);
    string name = "glDrawRangeElements(" + ToStr::Get(Count) + ")";

    uint32_t IdxSize = Type == eGL_UNSIGNED_BYTE ? 1 : Type == eGL_UNSIGNED_SHORT
                                                           ? 2
                                                           : /*Type == eGL_UNSIGNED_INT*/ 4;

    DrawcallDescription draw;
    draw.name = name;
    draw.numIndices = Count;
    draw.numInstances = 1;
    draw.indexOffset = uint32_t(IdxOffset) / IdxSize;
    draw.vertexOffset = 0;
    draw.instanceOffset = 0;

    draw.flags |= DrawFlags::Drawcall | DrawFlags::UseIBuffer;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);
    draw.indexByteWidth = IdxSize;

    AddDrawcall(draw, true);
  }

  return true;
}

void WrappedOpenGL::glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count,
                                        GLenum type, const void *indices)
{
  CoherentMapImplicitBarrier();

  m_Real.glDrawRangeElements(mode, start, end, count, type, indices);

  if(m_State == WRITING_CAPFRAME)
  {
    ClientMemoryData *clientMemory = CopyClientMemoryArrays(-1, count, type, indices);

    SCOPED_SERIALISE_CONTEXT(DRAWRANGEELEMENTS);
    Serialise_glDrawRangeElements(mode, start, end, count, type, indices);

    m_ContextRecord->AddChunk(scope.Get());

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);

    RestoreClientMemoryArrays(clientMemory, type);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
  }
}

bool WrappedOpenGL::Serialise_glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end,
                                                            GLsizei count, GLenum type,
                                                            const void *indices, GLint basevertex)
{
  SERIALISE_ELEMENT(GLenum, Mode, mode);
  SERIALISE_ELEMENT(uint32_t, Start, start);
  SERIALISE_ELEMENT(uint32_t, End, end);
  SERIALISE_ELEMENT(uint32_t, Count, count);
  SERIALISE_ELEMENT(GLenum, Type, type);
  SERIALISE_ELEMENT(uint64_t, IdxOffset, (uint64_t)indices);
  SERIALISE_ELEMENT(uint32_t, BaseVtx, basevertex);

  if(m_State <= EXECUTING)
  {
    Legacy_preElements(Type, Count);

    if(Check_preElements())
      m_Real.glDrawRangeElementsBaseVertex(Mode, Start, End, Count, Type, (const void *)IdxOffset,
                                           BaseVtx);
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);
    string name = "glDrawRangeElementsBaseVertex(" + ToStr::Get(Count) + ")";

    uint32_t IdxSize = Type == eGL_UNSIGNED_BYTE ? 1 : Type == eGL_UNSIGNED_SHORT
                                                           ? 2
                                                           : /*Type == eGL_UNSIGNED_INT*/ 4;

    DrawcallDescription draw;
    draw.name = name;
    draw.numIndices = Count;
    draw.numInstances = 1;
    draw.indexOffset = uint32_t(IdxOffset) / IdxSize;
    draw.baseVertex = BaseVtx;
    draw.instanceOffset = 0;

    draw.flags |= DrawFlags::Drawcall | DrawFlags::UseIBuffer;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);
    draw.indexByteWidth = IdxSize;

    AddDrawcall(draw, true);
  }

  return true;
}

void WrappedOpenGL::glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end,
                                                  GLsizei count, GLenum type, const void *indices,
                                                  GLint basevertex)
{
  CoherentMapImplicitBarrier();

  m_Real.glDrawRangeElementsBaseVertex(mode, start, end, count, type, indices, basevertex);

  if(m_State == WRITING_CAPFRAME)
  {
    ClientMemoryData *clientMemory = CopyClientMemoryArrays(-1, count, type, indices);

    SCOPED_SERIALISE_CONTEXT(DRAWRANGEELEMENTSBASEVERTEX);
    Serialise_glDrawRangeElementsBaseVertex(mode, start, end, count, type, indices, basevertex);

    m_ContextRecord->AddChunk(scope.Get());

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);

    RestoreClientMemoryArrays(clientMemory, type);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
  }
}

bool WrappedOpenGL::Serialise_glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type,
                                                       const void *indices, GLint basevertex)
{
  SERIALISE_ELEMENT(GLenum, Mode, mode);
  SERIALISE_ELEMENT(uint32_t, Count, count);
  SERIALISE_ELEMENT(GLenum, Type, type);
  SERIALISE_ELEMENT(uint64_t, IdxOffset, (uint64_t)indices);
  SERIALISE_ELEMENT(int32_t, BaseVtx, basevertex);

  if(m_State <= EXECUTING)
  {
    Legacy_preElements(Type, Count);

    if(Check_preElements())
      m_Real.glDrawElementsBaseVertex(Mode, Count, Type, (const void *)IdxOffset, BaseVtx);
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);
    string name = "glDrawElementsBaseVertex(" + ToStr::Get(Count) + ")";

    uint32_t IdxSize = Type == eGL_UNSIGNED_BYTE ? 1 : Type == eGL_UNSIGNED_SHORT
                                                           ? 2
                                                           : /*Type == eGL_UNSIGNED_INT*/ 4;

    DrawcallDescription draw;
    draw.name = name;
    draw.numIndices = Count;
    draw.numInstances = 1;
    draw.indexOffset = uint32_t(IdxOffset) / IdxSize;
    draw.baseVertex = BaseVtx;
    draw.instanceOffset = 0;

    draw.flags |= DrawFlags::Drawcall | DrawFlags::UseIBuffer;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);
    draw.indexByteWidth = IdxSize;

    AddDrawcall(draw, true);
  }

  return true;
}

void WrappedOpenGL::glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type,
                                             const void *indices, GLint basevertex)
{
  CoherentMapImplicitBarrier();

  m_Real.glDrawElementsBaseVertex(mode, count, type, indices, basevertex);

  if(m_State == WRITING_CAPFRAME)
  {
    ClientMemoryData *clientMemory = CopyClientMemoryArrays(-1, count, type, indices);

    SCOPED_SERIALISE_CONTEXT(DRAWELEMENTS_BASEVERTEX);
    Serialise_glDrawElementsBaseVertex(mode, count, type, indices, basevertex);

    m_ContextRecord->AddChunk(scope.Get());

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);

    RestoreClientMemoryArrays(clientMemory, type);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
  }
}

bool WrappedOpenGL::Serialise_glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type,
                                                      const void *indices, GLsizei instancecount)
{
  SERIALISE_ELEMENT(GLenum, Mode, mode);
  SERIALISE_ELEMENT(uint32_t, Count, count);
  SERIALISE_ELEMENT(GLenum, Type, type);
  SERIALISE_ELEMENT(uint64_t, IdxOffset, (uint64_t)indices);
  SERIALISE_ELEMENT(uint32_t, InstCount, instancecount);

  if(m_State <= EXECUTING)
  {
    Legacy_preElements(Type, Count);

    if(Check_preElements())
      m_Real.glDrawElementsInstanced(Mode, Count, Type, (const void *)IdxOffset, InstCount);
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);
    string name = "glDrawElementsInstanced(" + ToStr::Get(Count) + ")";

    uint32_t IdxSize = Type == eGL_UNSIGNED_BYTE ? 1 : Type == eGL_UNSIGNED_SHORT
                                                           ? 2
                                                           : /*Type == eGL_UNSIGNED_INT*/ 4;

    DrawcallDescription draw;
    draw.name = name;
    draw.numIndices = Count;
    draw.numInstances = InstCount;
    draw.indexOffset = uint32_t(IdxOffset) / IdxSize;
    draw.vertexOffset = 0;
    draw.instanceOffset = 0;

    draw.flags |= DrawFlags::Drawcall | DrawFlags::UseIBuffer;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);
    draw.indexByteWidth = IdxSize;

    AddDrawcall(draw, true);
  }

  return true;
}

void WrappedOpenGL::glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type,
                                            const void *indices, GLsizei instancecount)
{
  CoherentMapImplicitBarrier();

  m_Real.glDrawElementsInstanced(mode, count, type, indices, instancecount);

  if(m_State == WRITING_CAPFRAME)
  {
    ClientMemoryData *clientMemory = CopyClientMemoryArrays(-1, count, type, indices);

    SCOPED_SERIALISE_CONTEXT(DRAWELEMENTS_INSTANCED);
    Serialise_glDrawElementsInstanced(mode, count, type, indices, instancecount);

    m_ContextRecord->AddChunk(scope.Get());

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);

    RestoreClientMemoryArrays(clientMemory, type);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
  }
}

bool WrappedOpenGL::Serialise_glDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count,
                                                                  GLenum type, const void *indices,
                                                                  GLsizei instancecount,
                                                                  GLuint baseinstance)
{
  SERIALISE_ELEMENT(GLenum, Mode, mode);
  SERIALISE_ELEMENT(uint32_t, Count, count);
  SERIALISE_ELEMENT(GLenum, Type, type);
  SERIALISE_ELEMENT(uint64_t, IdxOffset, (uint64_t)indices);
  SERIALISE_ELEMENT(uint32_t, InstCount, instancecount);
  SERIALISE_ELEMENT(uint32_t, BaseInstance, baseinstance);

  if(m_State <= EXECUTING)
  {
    Legacy_preElements(Type, Count);

    if(Check_preElements())
      m_Real.glDrawElementsInstancedBaseInstance(Mode, Count, Type, (const void *)IdxOffset,
                                                 InstCount, BaseInstance);
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);
    string name = "glDrawElementsInstancedBaseInstance(" + ToStr::Get(Count) + ")";

    uint32_t IdxSize = Type == eGL_UNSIGNED_BYTE ? 1 : Type == eGL_UNSIGNED_SHORT
                                                           ? 2
                                                           : /*Type == eGL_UNSIGNED_INT*/ 4;

    DrawcallDescription draw;
    draw.name = name;
    draw.numIndices = Count;
    draw.numInstances = InstCount;
    draw.indexOffset = uint32_t(IdxOffset) / IdxSize;
    draw.vertexOffset = 0;
    draw.instanceOffset = BaseInstance;

    draw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::UseIBuffer;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);
    draw.indexByteWidth = IdxSize;

    AddDrawcall(draw, true);
  }

  return true;
}

void WrappedOpenGL::glDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count, GLenum type,
                                                        const void *indices, GLsizei instancecount,
                                                        GLuint baseinstance)
{
  CoherentMapImplicitBarrier();

  m_Real.glDrawElementsInstancedBaseInstance(mode, count, type, indices, instancecount, baseinstance);

  if(m_State == WRITING_CAPFRAME)
  {
    ClientMemoryData *clientMemory = CopyClientMemoryArrays(-1, count, type, indices);

    SCOPED_SERIALISE_CONTEXT(DRAWELEMENTS_INSTANCEDBASEINSTANCE);
    Serialise_glDrawElementsInstancedBaseInstance(mode, count, type, indices, instancecount,
                                                  baseinstance);

    m_ContextRecord->AddChunk(scope.Get());

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);

    RestoreClientMemoryArrays(clientMemory, type);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
  }
}

bool WrappedOpenGL::Serialise_glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count,
                                                                GLenum type, const void *indices,
                                                                GLsizei instancecount,
                                                                GLint basevertex)
{
  SERIALISE_ELEMENT(GLenum, Mode, mode);
  SERIALISE_ELEMENT(uint32_t, Count, count);
  SERIALISE_ELEMENT(GLenum, Type, type);
  SERIALISE_ELEMENT(uint64_t, IdxOffset, (uint64_t)indices);
  SERIALISE_ELEMENT(uint32_t, InstCount, instancecount);
  SERIALISE_ELEMENT(int32_t, BaseVertex, basevertex);

  if(m_State <= EXECUTING)
  {
    Legacy_preElements(Type, Count);

    if(Check_preElements())
      m_Real.glDrawElementsInstancedBaseVertex(Mode, Count, Type, (const void *)IdxOffset,
                                               InstCount, BaseVertex);
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);
    string name = "glDrawElementsInstancedBaseVertex(" + ToStr::Get(Count) + ", " +
                  ToStr::Get(InstCount) + ")";

    uint32_t IdxSize = Type == eGL_UNSIGNED_BYTE ? 1 : Type == eGL_UNSIGNED_SHORT
                                                           ? 2
                                                           : /*Type == eGL_UNSIGNED_INT*/ 4;

    DrawcallDescription draw;
    draw.name = name;
    draw.numIndices = Count;
    draw.numInstances = InstCount;
    draw.indexOffset = uint32_t(IdxOffset) / IdxSize;
    draw.baseVertex = BaseVertex;
    draw.instanceOffset = 0;

    draw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::UseIBuffer;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);
    draw.indexByteWidth = IdxSize;

    AddDrawcall(draw, true);
  }

  return true;
}

void WrappedOpenGL::glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type,
                                                      const void *indices, GLsizei instancecount,
                                                      GLint basevertex)
{
  CoherentMapImplicitBarrier();

  m_Real.glDrawElementsInstancedBaseVertex(mode, count, type, indices, instancecount, basevertex);

  if(m_State == WRITING_CAPFRAME)
  {
    ClientMemoryData *clientMemory = CopyClientMemoryArrays(-1, count, type, indices);

    SCOPED_SERIALISE_CONTEXT(DRAWELEMENTS_INSTANCEDBASEVERTEX);
    Serialise_glDrawElementsInstancedBaseVertex(mode, count, type, indices, instancecount,
                                                basevertex);

    m_ContextRecord->AddChunk(scope.Get());

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);

    RestoreClientMemoryArrays(clientMemory, type);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
  }
}

bool WrappedOpenGL::Serialise_glDrawElementsInstancedBaseVertexBaseInstance(
    GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount,
    GLint basevertex, GLuint baseinstance)
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
    Legacy_preElements(Type, Count);

    if(Check_preElements())
      m_Real.glDrawElementsInstancedBaseVertexBaseInstance(
          Mode, Count, Type, (const void *)IdxOffset, InstCount, BaseVertex, BaseInstance);
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);
    string name = "glDrawElementsInstancedBaseVertexBaseInstance(" + ToStr::Get(Count) + ", " +
                  ToStr::Get(InstCount) + ")";

    uint32_t IdxSize = Type == eGL_UNSIGNED_BYTE ? 1 : Type == eGL_UNSIGNED_SHORT
                                                           ? 2
                                                           : /*Type == eGL_UNSIGNED_INT*/ 4;

    DrawcallDescription draw;
    draw.name = name;
    draw.numIndices = Count;
    draw.numInstances = InstCount;
    draw.indexOffset = uint32_t(IdxOffset) / IdxSize;
    draw.baseVertex = BaseVertex;
    draw.instanceOffset = BaseInstance;

    draw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::UseIBuffer;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);
    draw.indexByteWidth = IdxSize;

    AddDrawcall(draw, true);
  }

  return true;
}

void WrappedOpenGL::glDrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei count,
                                                                  GLenum type, const void *indices,
                                                                  GLsizei instancecount,
                                                                  GLint basevertex,
                                                                  GLuint baseinstance)
{
  CoherentMapImplicitBarrier();

  m_Real.glDrawElementsInstancedBaseVertexBaseInstance(mode, count, type, indices, instancecount,
                                                       basevertex, baseinstance);

  if(m_State == WRITING_CAPFRAME)
  {
    ClientMemoryData *clientMemory = CopyClientMemoryArrays(-1, count, type, indices);

    SCOPED_SERIALISE_CONTEXT(DRAWELEMENTS_INSTANCEDBASEVERTEXBASEINSTANCE);
    Serialise_glDrawElementsInstancedBaseVertexBaseInstance(
        mode, count, type, indices, instancecount, basevertex, baseinstance);

    m_ContextRecord->AddChunk(scope.Get());

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);

    RestoreClientMemoryArrays(clientMemory, type);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
  }
}

bool WrappedOpenGL::Serialise_glMultiDrawArrays(GLenum mode, const GLint *first,
                                                const GLsizei *count, GLsizei drawcount)
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
    size_t i = 0;
    for(; i < m_Events.size(); i++)
    {
      if(m_Events[i].eventID >= m_CurEventID)
        break;
    }

    while(i > 1 && m_Events[i - 1].fileOffset == m_Events[i].fileOffset)
      i--;

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
      m_Real.glMultiDrawArrays(Mode, firstArray, countArray,
                               RDCMIN(Count, m_LastEventID - baseEventID + 1));
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
    string name = "glMultiDrawArrays(" + ToStr::Get(Count) + ")";

    DrawcallDescription draw;
    draw.name = name;
    draw.flags |= DrawFlags::MultiDraw;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);

    AddDrawcall(draw, false);

    m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());

    for(uint32_t i = 0; i < Count; i++)
    {
      m_CurEventID++;

      DrawcallDescription multidraw;
      multidraw.numIndices = countArray[i];
      multidraw.vertexOffset = firstArray[i];

      multidraw.name =
          "glMultiDrawArrays[" + ToStr::Get(i) + "](" + ToStr::Get(multidraw.numIndices) + ")";

      multidraw.flags |= DrawFlags::Drawcall;

      multidraw.topology = MakePrimitiveTopology(m_Real, Mode);

      AddEvent(desc);
      AddDrawcall(multidraw, true);
    }

    m_DrawcallStack.pop_back();
  }
  else
  {
    m_CurEventID += Count;
  }

  SAFE_DELETE_ARRAY(firstArray);
  SAFE_DELETE_ARRAY(countArray);

  return true;
}

void WrappedOpenGL::glMultiDrawArrays(GLenum mode, const GLint *first, const GLsizei *count,
                                      GLsizei drawcount)
{
  CoherentMapImplicitBarrier();

  m_Real.glMultiDrawArrays(mode, first, count, drawcount);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(MULTI_DRAWARRAYS);
    Serialise_glMultiDrawArrays(mode, first, count, drawcount);

    m_ContextRecord->AddChunk(scope.Get());

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
  }
}

bool WrappedOpenGL::Serialise_glMultiDrawElements(GLenum mode, const GLsizei *count, GLenum type,
                                                  const void *const *indices, GLsizei drawcount)
{
  SERIALISE_ELEMENT(GLenum, Mode, mode);
  SERIALISE_ELEMENT(GLenum, Type, type);
  SERIALISE_ELEMENT(uint32_t, Count, drawcount);

  SERIALISE_ELEMENT_ARR(int32_t, countArray, count, Count);

  void **idxOffsArray = new void *[Count];

  // serialise pointer array as uint64s
  if(m_State >= WRITING)
  {
    for(uint32_t i = 0; i < Count; i++)
    {
      uint64_t ptr = (uint64_t)indices[i];
      m_pSerialiser->Serialise("idxOffsArray", ptr);
    }
  }
  else
  {
    for(uint32_t i = 0; i < Count; i++)
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
    size_t i = 0;
    for(; i < m_Events.size(); i++)
    {
      if(m_Events[i].eventID >= m_CurEventID)
        break;
    }

    while(i > 1 && m_Events[i - 1].fileOffset == m_Events[i].fileOffset)
      i--;

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
      m_Real.glMultiDrawElements(Mode, countArray, Type, idxOffsArray,
                                 RDCMIN(Count, m_LastEventID - baseEventID + 1));
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
    string name = "glMultiDrawElements(" + ToStr::Get(Count) + ")";

    uint32_t IdxSize = Type == eGL_UNSIGNED_BYTE ? 1 : Type == eGL_UNSIGNED_SHORT
                                                           ? 2
                                                           : /*Type == eGL_UNSIGNED_INT*/ 4;

    DrawcallDescription draw;
    draw.name = name;

    draw.flags |= DrawFlags::MultiDraw;
    draw.indexByteWidth = IdxSize;
    draw.numIndices = 0;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);

    AddDrawcall(draw, false);

    m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());

    for(uint32_t i = 0; i < Count; i++)
    {
      m_CurEventID++;

      DrawcallDescription multidraw;
      multidraw.numIndices = countArray[i];
      multidraw.indexOffset = (uint32_t)uint64_t(idxOffsArray[i]) & 0xFFFFFFFF;
      multidraw.indexByteWidth = IdxSize;

      multidraw.indexOffset /= IdxSize;

      multidraw.name =
          "glMultiDrawElements[" + ToStr::Get(i) + "](" + ToStr::Get(multidraw.numIndices) + ")";

      multidraw.flags |= DrawFlags::Drawcall | DrawFlags::UseIBuffer;

      multidraw.topology = MakePrimitiveTopology(m_Real, Mode);

      AddEvent(desc);
      AddDrawcall(multidraw, true);
    }

    m_DrawcallStack.pop_back();
  }
  else
  {
    m_CurEventID += Count;
  }

  SAFE_DELETE_ARRAY(countArray);
  SAFE_DELETE_ARRAY(idxOffsArray);

  return true;
}

void WrappedOpenGL::glMultiDrawElements(GLenum mode, const GLsizei *count, GLenum type,
                                        const void *const *indices, GLsizei drawcount)
{
  CoherentMapImplicitBarrier();

  m_Real.glMultiDrawElements(mode, count, type, indices, drawcount);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(MULTI_DRAWELEMENTS);
    Serialise_glMultiDrawElements(mode, count, type, indices, drawcount);

    m_ContextRecord->AddChunk(scope.Get());

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
  }
}

bool WrappedOpenGL::Serialise_glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei *count,
                                                            GLenum type, const void *const *indices,
                                                            GLsizei drawcount,
                                                            const GLint *basevertex)
{
  SERIALISE_ELEMENT(GLenum, Mode, mode);
  SERIALISE_ELEMENT(GLenum, Type, type);
  SERIALISE_ELEMENT(uint32_t, Count, drawcount);

  SERIALISE_ELEMENT_ARR(int32_t, countArray, count, Count);
  SERIALISE_ELEMENT_ARR(int32_t, baseArray, basevertex, Count);

  void **idxOffsArray = new void *[Count];

  // serialise pointer array as uint64s
  if(m_State >= WRITING)
  {
    for(uint32_t i = 0; i < Count; i++)
    {
      uint64_t ptr = (uint64_t)indices[i];
      m_pSerialiser->Serialise("idxOffsArray", ptr);
    }
  }
  else
  {
    for(uint32_t i = 0; i < Count; i++)
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
    size_t i = 0;
    for(; i < m_Events.size(); i++)
    {
      if(m_Events[i].eventID >= m_CurEventID)
        break;
    }

    while(i > 1 && m_Events[i - 1].fileOffset == m_Events[i].fileOffset)
      i--;

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
      m_Real.glMultiDrawElementsBaseVertex(Mode, countArray, Type, idxOffsArray,
                                           RDCMIN(Count, m_LastEventID - baseEventID + 1), baseArray);
    }
    else
    {
      // otherwise we do the 'hard' case, draw only one multidraw
      // note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
      // a single draw.
      RDCASSERT(m_LastEventID == m_FirstEventID);

      uint32_t drawidx = (m_LastEventID - baseEventID);

      m_Real.glDrawElementsBaseVertex(Mode, countArray[drawidx], Type, idxOffsArray[drawidx],
                                      baseArray[drawidx]);
    }
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    string name = "glMultiDrawElementsBaseVertex(" + ToStr::Get(Count) + ")";

    uint32_t IdxSize = Type == eGL_UNSIGNED_BYTE ? 1 : Type == eGL_UNSIGNED_SHORT
                                                           ? 2
                                                           : /*Type == eGL_UNSIGNED_INT*/ 4;

    DrawcallDescription draw;
    draw.name = name;

    draw.flags |= DrawFlags::MultiDraw;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);
    draw.indexByteWidth = IdxSize;

    AddDrawcall(draw, false);

    m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());

    for(uint32_t i = 0; i < Count; i++)
    {
      m_CurEventID++;

      DrawcallDescription multidraw;
      multidraw.numIndices = countArray[i];
      multidraw.indexOffset = (uint32_t)uint64_t(idxOffsArray[i]) & 0xFFFFFFFF;
      multidraw.baseVertex = baseArray[i];

      multidraw.indexOffset /= IdxSize;

      multidraw.name = "glMultiDrawElementsBaseVertex[" + ToStr::Get(i) + "](" +
                       ToStr::Get(multidraw.numIndices) + ")";

      multidraw.flags |= DrawFlags::Drawcall | DrawFlags::UseIBuffer;

      multidraw.topology = MakePrimitiveTopology(m_Real, Mode);
      multidraw.indexByteWidth = IdxSize;

      AddEvent(desc);
      AddDrawcall(multidraw, true);
    }

    m_DrawcallStack.pop_back();
  }
  else
  {
    m_CurEventID += Count;
  }

  SAFE_DELETE_ARRAY(countArray);
  SAFE_DELETE_ARRAY(baseArray);
  SAFE_DELETE_ARRAY(idxOffsArray);

  return true;
}

void WrappedOpenGL::glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei *count, GLenum type,
                                                  const void *const *indices, GLsizei drawcount,
                                                  const GLint *basevertex)
{
  CoherentMapImplicitBarrier();

  m_Real.glMultiDrawElementsBaseVertex(mode, count, type, indices, drawcount, basevertex);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(MULTI_DRAWELEMENTSBASEVERTEX);
    Serialise_glMultiDrawElementsBaseVertex(mode, count, type, indices, drawcount, basevertex);

    m_ContextRecord->AddChunk(scope.Get());

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
  }
}

bool WrappedOpenGL::Serialise_glMultiDrawArraysIndirect(GLenum mode, const void *indirect,
                                                        GLsizei drawcount, GLsizei stride)
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
    size_t i = 0;
    for(; i < m_Events.size(); i++)
    {
      if(m_Events[i].eventID >= m_CurEventID)
        break;
    }

    while(i > 1 && m_Events[i - 1].fileOffset == m_Events[i].fileOffset)
      i--;

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
      m_Real.glMultiDrawArraysIndirect(Mode, (const void *)Offset,
                                       RDCMIN(Count, m_LastEventID - baseEventID + 1), Stride);
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
        offs += Stride * drawidx;
      else
        offs += sizeof(params) * drawidx;

      m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);

      m_Real.glDrawArraysInstancedBaseInstance(Mode, params.first, params.count,
                                               params.instanceCount, params.baseInstance);
    }
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    string name = "glMultiDrawArraysIndirect(" + ToStr::Get(Count) + ")";

    DrawcallDescription draw;
    draw.name = name;

    draw.flags |= DrawFlags::MultiDraw;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);

    AddDrawcall(draw, false);

    m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());

    {
      GLuint buf = 0;
      m_Real.glGetIntegerv(eGL_DRAW_INDIRECT_BUFFER_BINDING, (GLint *)&buf);

      m_ResourceUses[GetResourceManager()->GetID(BufferRes(GetCtx(), buf))].push_back(
          EventUsage(m_CurEventID, ResourceUsage::Indirect));
    }

    GLintptr offs = (GLintptr)Offset;

    for(uint32_t i = 0; i < Count; i++)
    {
      m_CurEventID++;

      DrawArraysIndirectCommand params;

      m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);

      if(Stride)
        offs += Stride;
      else
        offs += sizeof(params);

      DrawcallDescription multidraw;
      multidraw.numIndices = params.count;
      multidraw.numInstances = params.instanceCount;
      multidraw.vertexOffset = params.first;
      multidraw.instanceOffset = params.baseInstance;

      multidraw.name = "glMultiDrawArraysIndirect[" + ToStr::Get(i) + "](<" +
                       ToStr::Get(multidraw.numIndices) + ", " +
                       ToStr::Get(multidraw.numInstances) + ">)";

      multidraw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::Indirect;

      multidraw.topology = MakePrimitiveTopology(m_Real, Mode);

      AddEvent(desc);
      AddDrawcall(multidraw, true);
    }

    m_DrawcallStack.pop_back();
  }
  else
  {
    m_CurEventID += Count;
  }

  return true;
}

void WrappedOpenGL::glMultiDrawArraysIndirect(GLenum mode, const void *indirect, GLsizei drawcount,
                                              GLsizei stride)
{
  CoherentMapImplicitBarrier();

  m_Real.glMultiDrawArraysIndirect(mode, indirect, drawcount, stride);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(MULTI_DRAWARRAYS_INDIRECT);
    Serialise_glMultiDrawArraysIndirect(mode, indirect, drawcount, stride);

    m_ContextRecord->AddChunk(scope.Get());

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
  }
}

bool WrappedOpenGL::Serialise_glMultiDrawElementsIndirect(GLenum mode, GLenum type,
                                                          const void *indirect, GLsizei drawcount,
                                                          GLsizei stride)
{
  SERIALISE_ELEMENT(GLenum, Mode, mode);
  SERIALISE_ELEMENT(GLenum, Type, type);
  SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)indirect);
  SERIALISE_ELEMENT(uint32_t, Count, drawcount);
  SERIALISE_ELEMENT(uint32_t, Stride, stride);

  uint32_t IdxSize = Type == eGL_UNSIGNED_BYTE ? 1 : Type == eGL_UNSIGNED_SHORT
                                                         ? 2
                                                         : /*Type == eGL_UNSIGNED_INT*/ 4;

  if(m_State == READING)
  {
    m_Real.glMultiDrawElementsIndirect(Mode, Type, (const void *)Offset, Count, Stride);
  }
  else if(m_State <= EXECUTING)
  {
    size_t i = 0;
    for(; i < m_Events.size(); i++)
    {
      if(m_Events[i].eventID >= m_CurEventID)
        break;
    }

    while(i > 1 && m_Events[i - 1].fileOffset == m_Events[i].fileOffset)
      i--;

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
      m_Real.glMultiDrawElementsIndirect(Mode, Type, (const void *)Offset,
                                         RDCMIN(Count, m_LastEventID - baseEventID + 1), Stride);
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
        offs += Stride * drawidx;
      else
        offs += sizeof(params) * drawidx;

      m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);

      m_Real.glDrawElementsInstancedBaseVertexBaseInstance(
          Mode, params.count, Type, (const void *)ptrdiff_t(params.firstIndex * IdxSize),
          params.instanceCount, params.baseVertex, params.baseInstance);
    }
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    string name = "glMultiDrawElementsIndirect(" + ToStr::Get(Count) + ")";

    DrawcallDescription draw;
    draw.name = name;

    draw.flags |= DrawFlags::MultiDraw;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);
    draw.indexByteWidth = IdxSize;

    AddDrawcall(draw, false);

    m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());

    {
      GLuint buf = 0;
      m_Real.glGetIntegerv(eGL_DRAW_INDIRECT_BUFFER_BINDING, (GLint *)&buf);

      m_ResourceUses[GetResourceManager()->GetID(BufferRes(GetCtx(), buf))].push_back(
          EventUsage(m_CurEventID, ResourceUsage::Indirect));
    }

    GLintptr offs = (GLintptr)Offset;

    for(uint32_t i = 0; i < Count; i++)
    {
      m_CurEventID++;

      DrawElementsIndirectCommand params;

      m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);

      if(Stride)
        offs += Stride;
      else
        offs += sizeof(params);

      DrawcallDescription multidraw;
      multidraw.numIndices = params.count;
      multidraw.numInstances = params.instanceCount;
      multidraw.indexOffset = params.firstIndex;
      multidraw.baseVertex = params.baseVertex;
      multidraw.instanceOffset = params.baseInstance;

      multidraw.name = "glMultiDrawElementsIndirect[" + ToStr::Get(i) + "](<" +
                       ToStr::Get(multidraw.numIndices) + ", " +
                       ToStr::Get(multidraw.numInstances) + ">)";

      multidraw.flags |=
          DrawFlags::Drawcall | DrawFlags::UseIBuffer | DrawFlags::Instanced | DrawFlags::Indirect;

      multidraw.topology = MakePrimitiveTopology(m_Real, Mode);
      multidraw.indexByteWidth = IdxSize;

      AddEvent(desc);
      AddDrawcall(multidraw, true);
    }

    m_DrawcallStack.pop_back();
  }
  else
  {
    m_CurEventID += Count;
  }

  return true;
}

void WrappedOpenGL::glMultiDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect,
                                                GLsizei drawcount, GLsizei stride)
{
  CoherentMapImplicitBarrier();

  m_Real.glMultiDrawElementsIndirect(mode, type, indirect, drawcount, stride);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(MULTI_DRAWELEMENTS_INDIRECT);
    Serialise_glMultiDrawElementsIndirect(mode, type, indirect, drawcount, stride);

    m_ContextRecord->AddChunk(scope.Get());

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
  }
}

bool WrappedOpenGL::Serialise_glMultiDrawArraysIndirectCountARB(GLenum mode, GLintptr indirect,
                                                                GLintptr drawcount,
                                                                GLsizei maxdrawcount, GLsizei stride)
{
  SERIALISE_ELEMENT(GLenum, Mode, mode);
  SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)indirect);
  SERIALISE_ELEMENT(uint64_t, Count, (uint64_t)drawcount);
  SERIALISE_ELEMENT(uint32_t, MaxCount, maxdrawcount);
  SERIALISE_ELEMENT(uint32_t, Stride, stride);

  uint32_t realdrawcount = 0;

  if(m_State < WRITING)
  {
    m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, (GLintptr)Count, sizeof(realdrawcount),
                              &realdrawcount);

    realdrawcount = RDCMIN(MaxCount, realdrawcount);
  }

  if(m_State == READING)
  {
    m_Real.glMultiDrawArraysIndirectCountARB(Mode, (GLintptr)Offset, (GLintptr)Count, MaxCount,
                                             Stride);
  }
  else if(m_State <= EXECUTING)
  {
    size_t i = 0;
    for(; i < m_Events.size(); i++)
    {
      if(m_Events[i].eventID >= m_CurEventID)
        break;
    }

    while(i > 1 && m_Events[i - 1].fileOffset == m_Events[i].fileOffset)
      i--;

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
      m_Real.glMultiDrawArraysIndirect(
          Mode, (const void *)Offset, RDCMIN(realdrawcount, m_LastEventID - baseEventID + 1), Stride);
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
        offs += Stride * drawidx;
      else
        offs += sizeof(params) * drawidx;

      m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);

      m_Real.glDrawArraysInstancedBaseInstance(Mode, params.first, params.count,
                                               params.instanceCount, params.baseInstance);
    }
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    string name = "glMultiDrawArraysIndirectCountARB(<" + ToStr::Get(realdrawcount) + ">)";

    DrawcallDescription draw;
    draw.name = name;

    draw.flags |= DrawFlags::MultiDraw;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);

    AddDrawcall(draw, false);

    m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());

    {
      GLuint buf = 0;
      m_Real.glGetIntegerv(eGL_DRAW_INDIRECT_BUFFER_BINDING, (GLint *)&buf);

      m_ResourceUses[GetResourceManager()->GetID(BufferRes(GetCtx(), buf))].push_back(
          EventUsage(m_CurEventID, ResourceUsage::Indirect));
    }

    GLintptr offs = (GLintptr)Offset;

    for(uint32_t i = 0; i < realdrawcount; i++)
    {
      m_CurEventID++;

      DrawArraysIndirectCommand params;

      m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);

      if(Stride)
        offs += Stride;
      else
        offs += sizeof(params);

      DrawcallDescription multidraw;
      multidraw.numIndices = params.count;
      multidraw.numInstances = params.instanceCount;
      multidraw.vertexOffset = params.first;
      multidraw.instanceOffset = params.baseInstance;

      multidraw.name = "glMultiDrawArraysIndirect[" + ToStr::Get(i) + "](<" +
                       ToStr::Get(multidraw.numIndices) + ", " +
                       ToStr::Get(multidraw.numInstances) + ">)";

      multidraw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::Indirect;

      multidraw.topology = MakePrimitiveTopology(m_Real, Mode);

      AddEvent(desc);
      AddDrawcall(multidraw, true);
    }

    m_DrawcallStack.pop_back();
  }
  else
  {
    m_CurEventID += realdrawcount;
  }

  return true;
}

void WrappedOpenGL::glMultiDrawArraysIndirectCountARB(GLenum mode, GLintptr indirect,
                                                      GLintptr drawcount, GLsizei maxdrawcount,
                                                      GLsizei stride)
{
  CoherentMapImplicitBarrier();

  m_Real.glMultiDrawArraysIndirectCountARB(mode, indirect, drawcount, maxdrawcount, stride);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(MULTI_DRAWARRAYS_INDIRECT_COUNT);
    Serialise_glMultiDrawArraysIndirectCountARB(mode, indirect, drawcount, maxdrawcount, stride);

    m_ContextRecord->AddChunk(scope.Get());

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
  }
}

bool WrappedOpenGL::Serialise_glMultiDrawElementsIndirectCountARB(GLenum mode, GLenum type,
                                                                  GLintptr indirect,
                                                                  GLintptr drawcount,
                                                                  GLsizei maxdrawcount,
                                                                  GLsizei stride)
{
  SERIALISE_ELEMENT(GLenum, Mode, mode);
  SERIALISE_ELEMENT(GLenum, Type, type);
  SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)indirect);
  SERIALISE_ELEMENT(uint64_t, Count, (uint64_t)drawcount);
  SERIALISE_ELEMENT(uint32_t, MaxCount, maxdrawcount);
  SERIALISE_ELEMENT(uint32_t, Stride, stride);

  uint32_t IdxSize = Type == eGL_UNSIGNED_BYTE ? 1 : Type == eGL_UNSIGNED_SHORT
                                                         ? 2
                                                         : /*Type == eGL_UNSIGNED_INT*/ 4;

  uint32_t realdrawcount = 0;

  if(m_State < WRITING)
  {
    m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, (GLintptr)Count, sizeof(realdrawcount),
                              &realdrawcount);

    realdrawcount = RDCMIN(MaxCount, realdrawcount);
  }

  if(m_State == READING)
  {
    m_Real.glMultiDrawElementsIndirectCountARB(Mode, Type, (GLintptr)Offset, (GLintptr)Count,
                                               MaxCount, Stride);
  }
  else if(m_State <= EXECUTING)
  {
    size_t i = 0;
    for(; i < m_Events.size(); i++)
    {
      if(m_Events[i].eventID >= m_CurEventID)
        break;
    }

    while(i > 1 && m_Events[i - 1].fileOffset == m_Events[i].fileOffset)
      i--;

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
      m_Real.glMultiDrawElementsIndirect(Mode, Type, (const void *)Offset,
                                         RDCMIN(realdrawcount, m_LastEventID - baseEventID + 1),
                                         Stride);
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
        offs += Stride * drawidx;
      else
        offs += sizeof(params) * drawidx;

      m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);

      m_Real.glDrawElementsInstancedBaseVertexBaseInstance(
          Mode, params.count, Type, (const void *)ptrdiff_t(params.firstIndex * IdxSize),
          params.instanceCount, params.baseVertex, params.baseInstance);
    }
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    string name = "glMultiDrawElementsIndirectCountARB(<" + ToStr::Get(realdrawcount) + ">)";

    DrawcallDescription draw;
    draw.name = name;

    draw.flags |= DrawFlags::MultiDraw;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);
    draw.indexByteWidth = IdxSize;

    AddDrawcall(draw, false);

    m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());

    {
      GLuint buf = 0;
      m_Real.glGetIntegerv(eGL_DRAW_INDIRECT_BUFFER_BINDING, (GLint *)&buf);

      m_ResourceUses[GetResourceManager()->GetID(BufferRes(GetCtx(), buf))].push_back(
          EventUsage(m_CurEventID, ResourceUsage::Indirect));
    }

    GLintptr offs = (GLintptr)Offset;

    for(uint32_t i = 0; i < realdrawcount; i++)
    {
      m_CurEventID++;

      DrawElementsIndirectCommand params;

      m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);

      if(Stride)
        offs += Stride;
      else
        offs += sizeof(params);

      DrawcallDescription multidraw;
      multidraw.numIndices = params.count;
      multidraw.numInstances = params.instanceCount;
      multidraw.indexOffset = params.firstIndex;
      multidraw.baseVertex = params.baseVertex;
      multidraw.instanceOffset = params.baseInstance;

      multidraw.name = "glMultiDrawElementsIndirect[" + ToStr::Get(i) + "](" +
                       ToStr::Get(multidraw.numIndices) + ", " +
                       ToStr::Get(multidraw.numInstances) + ")";

      multidraw.flags |=
          DrawFlags::Drawcall | DrawFlags::UseIBuffer | DrawFlags::Instanced | DrawFlags::Indirect;

      multidraw.topology = MakePrimitiveTopology(m_Real, Mode);
      multidraw.indexByteWidth = IdxSize;

      AddEvent(desc);
      AddDrawcall(multidraw, true);
    }

    m_DrawcallStack.pop_back();
  }
  else
  {
    m_CurEventID += realdrawcount;
  }

  return true;
}

void WrappedOpenGL::glMultiDrawElementsIndirectCountARB(GLenum mode, GLenum type, GLintptr indirect,
                                                        GLintptr drawcount, GLsizei maxdrawcount,
                                                        GLsizei stride)
{
  CoherentMapImplicitBarrier();

  m_Real.glMultiDrawElementsIndirectCountARB(mode, type, indirect, drawcount, maxdrawcount, stride);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(MULTI_DRAWELEMENTS_INDIRECT_COUNT);
    Serialise_glMultiDrawElementsIndirectCountARB(mode, type, indirect, drawcount, maxdrawcount,
                                                  stride);

    m_ContextRecord->AddChunk(scope.Get());

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
  }
}

bool WrappedOpenGL::Serialise_glClearNamedFramebufferfv(GLuint framebuffer, GLenum buffer,
                                                        GLint drawbuffer, const GLfloat *value)
{
  SERIALISE_ELEMENT(ResourceId, Id,
                    (framebuffer ? GetResourceManager()->GetID(FramebufferRes(GetCtx(), framebuffer))
                                 : ResourceId()));
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
    if(value)
      v = *((Vec4f *)value);

    m_pSerialiser->SerialisePODArray<4>("value", (float *)&v.x);

    if(m_State == READING)
      name = "glClearBufferfv(" + ToStr::Get(buf) + ", " + ToStr::Get(drawbuf) + ", " +
             ToStr::Get(v.x) + ", " + ToStr::Get(v.y) + ", " + ToStr::Get(v.z) + ", " +
             ToStr::Get(v.w) + ")";

    // use ARB_direct_state_access functions here as we use EXT_direct_state_access elsewhere. If
    // we are running without ARB_dsa support, these functions are emulated in the obvious way. This
    // is
    // necessary since these functions can be serialised even if ARB_dsa was not used originally,
    // and
    // we need to support this case.
    if(m_State <= EXECUTING)
      m_Real.glClearNamedFramebufferfv(framebuffer, buf, drawbuf, &v.x);
  }
  else
  {
    SERIALISE_ELEMENT(float, val, *value);

    if(m_State == READING)
      name = "glClearBufferfv(" + ToStr::Get(buf) + ", " + ToStr::Get(drawbuf) + ", " +
             ToStr::Get(val) + ")";

    if(m_State <= EXECUTING)
      m_Real.glClearNamedFramebufferfv(framebuffer, buf, drawbuf, &val);
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);

    DrawcallDescription draw;
    draw.name = name;
    draw.flags |= DrawFlags::Clear;
    if(buf == eGL_COLOR)
      draw.flags |= DrawFlags::ClearColor;
    else
      draw.flags |= DrawFlags::ClearDepthStencil;

    GLuint attachment = 0;
    GLenum attachName =
        buf == eGL_COLOR ? GLenum(eGL_COLOR_ATTACHMENT0 + drawbuf) : eGL_DEPTH_ATTACHMENT;
    GLenum type = eGL_TEXTURE;
    m_Real.glGetNamedFramebufferAttachmentParameterivEXT(
        framebuffer, attachName, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&attachment);
    m_Real.glGetNamedFramebufferAttachmentParameterivEXT(
        framebuffer, attachName, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

    if(attachment)
    {
      ResourceId id;

      if(type == eGL_TEXTURE)
        id = GetResourceManager()->GetID(TextureRes(GetCtx(), attachment));
      else
        id = GetResourceManager()->GetID(RenderbufferRes(GetCtx(), attachment));

      m_ResourceUses[id].push_back(EventUsage(m_CurEventID, ResourceUsage::Clear));
      draw.copyDestination = GetResourceManager()->GetOriginalID(id);
    }

    AddDrawcall(draw, true);
  }

  return true;
}

void WrappedOpenGL::glClearNamedFramebufferfv(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                              const GLfloat *value)
{
  CoherentMapImplicitBarrier();

  m_Real.glClearNamedFramebufferfv(framebuffer, buffer, drawbuffer, value);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(CLEARBUFFERF);
    Serialise_glClearNamedFramebufferfv(framebuffer, buffer, drawbuffer, value);

    m_ContextRecord->AddChunk(scope.Get());

    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.FetchState(GetCtx(), this);
    state.MarkReferenced(this, false);
  }
  else if(m_State == WRITING_IDLE)
  {
    GLRenderState state(&m_Real, m_pSerialiser, m_State);
    state.MarkDirty(this);
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

bool WrappedOpenGL::Serialise_glClearNamedFramebufferiv(GLuint framebuffer, GLenum buffer,
                                                        GLint drawbuffer, const GLint *value)
{
  SERIALISE_ELEMENT(ResourceId, Id,
                    (framebuffer ? GetResourceManager()->GetID(FramebufferRes(GetCtx(), framebuffer))
                                 : ResourceId()));
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
    if(value)
      memcpy(v, value, sizeof(v));

    m_pSerialiser->SerialisePODArray<4>("value", v);

    if(m_State == READING)
      name = "glClearBufferiv(" + ToStr::Get(buf) + ", " + ToStr::Get(drawbuf) + ", " +
             ToStr::Get(v[0]) + ", " + ToStr::Get(v[1]) + ", " + ToStr::Get(v[2]) + ", " +
             ToStr::Get(v[3]) + ")";

    // use ARB_direct_state_access functions here as we use EXT_direct_state_access elsewhere. If
    // we are running without ARB_dsa support, these functions are emulated in the obvious way. This
    // is
    // necessary since these functions can be serialised even if ARB_dsa was not used originally,
    // and
    // we need to support this case.
    if(m_State <= EXECUTING)
      m_Real.glClearNamedFramebufferiv(framebuffer, buf, drawbuf, v);
  }
  else
  {
    SERIALISE_ELEMENT(int32_t, val, *value);

    if(m_State == READING)
      name = "glClearBufferiv(" + ToStr::Get(buf) + ", " + ToStr::Get(drawbuf) + ", " +
             ToStr::Get(val) + ")";

    if(m_State <= EXECUTING)
      m_Real.glClearNamedFramebufferiv(framebuffer, buf, drawbuf, &val);
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);

    DrawcallDescription draw;
    draw.name = name;
    draw.flags |= DrawFlags::Clear;
    if(buf == eGL_COLOR)
      draw.flags |= DrawFlags::ClearColor;
    else
      draw.flags |= DrawFlags::ClearDepthStencil;

    GLuint attachment = 0;
    GLenum attachName =
        buf == eGL_COLOR ? GLenum(eGL_COLOR_ATTACHMENT0 + drawbuf) : eGL_STENCIL_ATTACHMENT;
    GLenum type = eGL_TEXTURE;
    m_Real.glGetNamedFramebufferAttachmentParameterivEXT(
        framebuffer, attachName, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&attachment);
    m_Real.glGetNamedFramebufferAttachmentParameterivEXT(
        framebuffer, attachName, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

    if(attachment)
    {
      ResourceId id;

      if(type == eGL_TEXTURE)
        id = GetResourceManager()->GetID(TextureRes(GetCtx(), attachment));
      else
        id = GetResourceManager()->GetID(RenderbufferRes(GetCtx(), attachment));

      m_ResourceUses[id].push_back(EventUsage(m_CurEventID, ResourceUsage::Clear));
      draw.copyDestination = GetResourceManager()->GetOriginalID(id);
    }

    AddDrawcall(draw, true);
  }

  return true;
}

void WrappedOpenGL::glClearNamedFramebufferiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                              const GLint *value)
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

bool WrappedOpenGL::Serialise_glClearNamedFramebufferuiv(GLuint framebuffer, GLenum buffer,
                                                         GLint drawbuffer, const GLuint *value)
{
  SERIALISE_ELEMENT(ResourceId, Id,
                    (framebuffer ? GetResourceManager()->GetID(FramebufferRes(GetCtx(), framebuffer))
                                 : ResourceId()));
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
    if(value)
      memcpy(v, value, sizeof(v));

    m_pSerialiser->SerialisePODArray<4>("value", v);

    if(m_State == READING)
      name = "glClearBufferuiv(" + ToStr::Get(buf) + ", " + ToStr::Get(drawbuf) + ", " +
             ToStr::Get(v[0]) + ", " + ToStr::Get(v[1]) + ", " + ToStr::Get(v[2]) + ", " +
             ToStr::Get(v[3]) + ")";

    // use ARB_direct_state_access functions here as we use EXT_direct_state_access elsewhere. If
    // we are running without ARB_dsa support, these functions are emulated in the obvious way. This
    // is
    // necessary since these functions can be serialised even if ARB_dsa was not used originally,
    // and
    // we need to support this case.
    if(m_State <= EXECUTING)
      m_Real.glClearNamedFramebufferuiv(framebuffer, buf, drawbuf, v);
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);

    DrawcallDescription draw;
    draw.name = name;
    draw.flags |= DrawFlags::Clear | DrawFlags::ClearColor;

    GLuint attachment = 0;
    GLenum attachName = GLenum(eGL_COLOR_ATTACHMENT0 + drawbuf);
    GLenum type = eGL_TEXTURE;
    m_Real.glGetNamedFramebufferAttachmentParameterivEXT(
        framebuffer, attachName, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&attachment);
    m_Real.glGetNamedFramebufferAttachmentParameterivEXT(
        framebuffer, attachName, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

    if(attachment)
    {
      ResourceId id;

      if(type == eGL_TEXTURE)
        id = GetResourceManager()->GetID(TextureRes(GetCtx(), attachment));
      else
        id = GetResourceManager()->GetID(RenderbufferRes(GetCtx(), attachment));

      m_ResourceUses[id].push_back(EventUsage(m_CurEventID, ResourceUsage::Clear));
      draw.copyDestination = GetResourceManager()->GetOriginalID(id);
    }

    AddDrawcall(draw, true);
  }

  return true;
}

void WrappedOpenGL::glClearNamedFramebufferuiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                               const GLuint *value)
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

bool WrappedOpenGL::Serialise_glClearNamedFramebufferfi(GLuint framebuffer, GLenum buffer,
                                                        GLfloat depth, GLint stencil)
{
  SERIALISE_ELEMENT(ResourceId, Id,
                    (framebuffer ? GetResourceManager()->GetID(FramebufferRes(GetCtx(), framebuffer))
                                 : ResourceId()));
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
  // we are running without ARB_dsa support, these functions are emulated in the obvious way. This
  // is
  // necessary since these functions can be serialised even if ARB_dsa was not used originally, and
  // we need to support this case.
  if(m_State <= EXECUTING)
    m_Real.glClearNamedFramebufferfi(framebuffer, buf, d, s);

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);
    string name = "glClearBufferfi(" + ToStr::Get(d) + ToStr::Get(s) + ")";

    DrawcallDescription draw;
    draw.name = name;
    draw.flags |= DrawFlags::Clear | DrawFlags::ClearDepthStencil;

    GLuint attachment = 0;
    GLenum type = eGL_TEXTURE;
    m_Real.glGetNamedFramebufferAttachmentParameterivEXT(framebuffer, eGL_DEPTH_ATTACHMENT,
                                                         eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                         (GLint *)&attachment);
    m_Real.glGetNamedFramebufferAttachmentParameterivEXT(
        framebuffer, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

    if(attachment)
    {
      ResourceId id;

      if(type == eGL_TEXTURE)
        id = GetResourceManager()->GetID(TextureRes(GetCtx(), attachment));
      else
        id = GetResourceManager()->GetID(RenderbufferRes(GetCtx(), attachment));

      m_ResourceUses[id].push_back(EventUsage(m_CurEventID, ResourceUsage::Clear));
      draw.copyDestination = GetResourceManager()->GetOriginalID(id);
    }

    AddDrawcall(draw, true);

    attachment = 0;
    type = eGL_TEXTURE;
    m_Real.glGetNamedFramebufferAttachmentParameterivEXT(framebuffer, eGL_STENCIL_ATTACHMENT,
                                                         eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                         (GLint *)&attachment);
    m_Real.glGetNamedFramebufferAttachmentParameterivEXT(
        framebuffer, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

    if(attachment)
    {
      if(type == eGL_TEXTURE)
        m_ResourceUses[GetResourceManager()->GetID(TextureRes(GetCtx(), attachment))].push_back(
            EventUsage(m_CurEventID, ResourceUsage::Clear));
      else
        m_ResourceUses[GetResourceManager()->GetID(RenderbufferRes(GetCtx(), attachment))].push_back(
            EventUsage(m_CurEventID, ResourceUsage::Clear));
    }
  }

  return true;
}

void WrappedOpenGL::glClearNamedFramebufferfi(GLuint framebuffer, GLenum buffer, GLfloat depth,
                                              GLint stencil)
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

bool WrappedOpenGL::Serialise_glClearNamedBufferDataEXT(GLuint buffer, GLenum internalformat,
                                                        GLenum format, GLenum type, const void *data)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(BufferRes(GetCtx(), buffer)));
  SERIALISE_ELEMENT(GLenum, InternalFormat, internalformat);
  SERIALISE_ELEMENT(GLenum, Format, format);
  SERIALISE_ELEMENT(GLenum, Type, type);

  uint64_t val[4] = {0};

  if(m_State >= WRITING && data != NULL)
  {
    size_t s = 1;
    switch(Format)
    {
      default:
        RDCWARN("Unexpected format %x, defaulting to single component", Format);
      // fall through
      case eGL_RED:
      case eGL_RED_INTEGER:
      case eGL_GREEN_INTEGER:
      case eGL_BLUE_INTEGER:
      case eGL_DEPTH_COMPONENT:
      case eGL_STENCIL_INDEX: s = 1; break;
      case eGL_RG:
      case eGL_RG_INTEGER:
      case eGL_DEPTH_STENCIL: s = 2; break;
      case eGL_RGB:
      case eGL_BGR:
      case eGL_RGB_INTEGER:
      case eGL_BGR_INTEGER: s = 3; break;
      case eGL_RGBA:
      case eGL_BGRA:
      case eGL_RGBA_INTEGER:
      case eGL_BGRA_INTEGER: s = 4; break;
    }
    switch(Type)
    {
      case eGL_UNSIGNED_BYTE:
      case eGL_BYTE: s *= 1; break;
      case eGL_UNSIGNED_SHORT:
      case eGL_SHORT: s *= 2; break;
      case eGL_UNSIGNED_INT:
      case eGL_INT:
      case eGL_FLOAT: s *= 4; break;
      default:
        RDCWARN("Unexpected type %x, defaulting to 1 byte type", Format);
      // fall through
      case eGL_UNSIGNED_BYTE_3_3_2:
      case eGL_UNSIGNED_BYTE_2_3_3_REV: s = 1; break;
      case eGL_UNSIGNED_SHORT_5_6_5:
      case eGL_UNSIGNED_SHORT_5_6_5_REV:
      case eGL_UNSIGNED_SHORT_4_4_4_4:
      case eGL_UNSIGNED_SHORT_4_4_4_4_REV:
      case eGL_UNSIGNED_SHORT_5_5_5_1:
      case eGL_UNSIGNED_SHORT_1_5_5_5_REV:
      case eGL_UNSIGNED_INT_8_8_8_8:
      case eGL_UNSIGNED_INT_8_8_8_8_REV: s = 2; break;
      case eGL_UNSIGNED_INT_10_10_10_2:
      case eGL_UNSIGNED_INT_2_10_10_10_REV: s = 4; break;
    }
    memcpy(val, data, s);
  }

  m_pSerialiser->SerialisePODArray<4>("data", val);

  if(m_State <= EXECUTING)
  {
    m_Real.glClearNamedBufferDataEXT(GetResourceManager()->GetLiveResource(id).name, InternalFormat,
                                     Format, Type, (const void *)&val[0]);
  }

  return true;
}

void WrappedOpenGL::glClearNamedBufferDataEXT(GLuint buffer, GLenum internalformat, GLenum format,
                                              GLenum type, const void *data)
{
  CoherentMapImplicitBarrier();

  m_Real.glClearNamedBufferDataEXT(buffer, internalformat, format, type, data);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(CLEARBUFFERDATA);
    Serialise_glClearNamedBufferDataEXT(buffer, internalformat, format, type, data);

    m_ContextRecord->AddChunk(scope.Get());
  }
  else if(m_State == WRITING_IDLE)
  {
    GetResourceManager()->MarkDirtyResource(BufferRes(GetCtx(), buffer));
  }
}

void WrappedOpenGL::glClearBufferData(GLenum target, GLenum internalformat, GLenum format,
                                      GLenum type, const void *data)
{
  CoherentMapImplicitBarrier();

  m_Real.glClearBufferData(target, internalformat, format, type, data);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetCtxData().m_BufferRecord[BufferIdx(target)];
    RDCASSERTMSG("Couldn't identify implicit object at binding. Mismatched or bad GLuint?", record,
                 target);

    if(record)
    {
      if(m_State == WRITING_CAPFRAME)
      {
        SCOPED_SERIALISE_CONTEXT(CLEARBUFFERDATA);
        Serialise_glClearNamedBufferDataEXT(record->Resource.name, internalformat, format, type,
                                            data);

        m_ContextRecord->AddChunk(scope.Get());
      }
      else if(m_State == WRITING_IDLE)
      {
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      }
    }
  }
}

bool WrappedOpenGL::Serialise_glClearNamedBufferSubDataEXT(GLuint buffer, GLenum internalformat,
                                                           GLintptr offset, GLsizeiptr size,
                                                           GLenum format, GLenum type,
                                                           const void *data)
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
      // fall through
      case eGL_RED:
      case eGL_RED_INTEGER:
      case eGL_GREEN_INTEGER:
      case eGL_BLUE_INTEGER:
      case eGL_DEPTH_COMPONENT:
      case eGL_STENCIL_INDEX: s = 1; break;
      case eGL_RG:
      case eGL_RG_INTEGER:
      case eGL_DEPTH_STENCIL: s = 2; break;
      case eGL_RGB:
      case eGL_BGR:
      case eGL_RGB_INTEGER:
      case eGL_BGR_INTEGER: s = 3; break;
      case eGL_RGBA:
      case eGL_BGRA:
      case eGL_RGBA_INTEGER:
      case eGL_BGRA_INTEGER: s = 4; break;
    }
    switch(Type)
    {
      case eGL_UNSIGNED_BYTE:
      case eGL_BYTE: s *= 1; break;
      case eGL_UNSIGNED_SHORT:
      case eGL_SHORT: s *= 2; break;
      case eGL_UNSIGNED_INT:
      case eGL_INT:
      case eGL_FLOAT: s *= 4; break;
      default:
        RDCWARN("Unexpected type %x, defaulting to 1 byte type", Format);
      // fall through
      case eGL_UNSIGNED_BYTE_3_3_2:
      case eGL_UNSIGNED_BYTE_2_3_3_REV: s = 1; break;
      case eGL_UNSIGNED_SHORT_5_6_5:
      case eGL_UNSIGNED_SHORT_5_6_5_REV:
      case eGL_UNSIGNED_SHORT_4_4_4_4:
      case eGL_UNSIGNED_SHORT_4_4_4_4_REV:
      case eGL_UNSIGNED_SHORT_5_5_5_1:
      case eGL_UNSIGNED_SHORT_1_5_5_5_REV:
      case eGL_UNSIGNED_INT_8_8_8_8:
      case eGL_UNSIGNED_INT_8_8_8_8_REV: s = 2; break;
      case eGL_UNSIGNED_INT_10_10_10_2:
      case eGL_UNSIGNED_INT_2_10_10_10_REV: s = 4; break;
    }
    memcpy(val, data, s);
  }

  m_pSerialiser->SerialisePODArray<4>("data", val);

  if(m_State <= EXECUTING)
  {
    m_Real.glClearNamedBufferSubDataEXT(GetResourceManager()->GetLiveResource(id).name,
                                        InternalFormat, (GLintptr)Offset, (GLsizeiptr)Size, Format,
                                        Type, (const void *)&val[0]);
  }

  return true;
}

void WrappedOpenGL::glClearNamedBufferSubDataEXT(GLuint buffer, GLenum internalformat,
                                                 GLintptr offset, GLsizeiptr size, GLenum format,
                                                 GLenum type, const void *data)
{
  CoherentMapImplicitBarrier();

  m_Real.glClearNamedBufferSubDataEXT(buffer, internalformat, offset, size, format, type, data);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(CLEARBUFFERSUBDATA);
    Serialise_glClearNamedBufferSubDataEXT(buffer, internalformat, offset, size, format, type, data);

    m_ContextRecord->AddChunk(scope.Get());
  }
  else if(m_State == WRITING_IDLE)
  {
    GetResourceManager()->MarkDirtyResource(BufferRes(GetCtx(), buffer));
  }
}

void WrappedOpenGL::glClearNamedBufferSubData(GLuint buffer, GLenum internalformat, GLintptr offset,
                                              GLsizeiptr size, GLenum format, GLenum type,
                                              const void *data)
{
  // only difference to EXT function is size parameter, so just upcast
  glClearNamedBufferSubDataEXT(buffer, internalformat, offset, size, format, type, data);
}

void WrappedOpenGL::glClearBufferSubData(GLenum target, GLenum internalformat, GLintptr offset,
                                         GLsizeiptr size, GLenum format, GLenum type,
                                         const void *data)
{
  CoherentMapImplicitBarrier();

  m_Real.glClearBufferSubData(target, internalformat, offset, size, format, type, data);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetCtxData().m_BufferRecord[BufferIdx(target)];
    RDCASSERTMSG("Couldn't identify implicit object at binding. Mismatched or bad GLuint?", record,
                 target);

    if(record)
    {
      if(m_State == WRITING_CAPFRAME)
      {
        SCOPED_SERIALISE_CONTEXT(CLEARBUFFERSUBDATA);
        Serialise_glClearNamedBufferSubDataEXT(record->Resource.name, internalformat, offset, size,
                                               format, type, data);

        m_ContextRecord->AddChunk(scope.Get());
      }
      else if(m_State == WRITING_IDLE)
      {
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      }
    }
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
    AddEvent(desc);
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

    if(Mask & (eGL_DEPTH_BUFFER_BIT | eGL_COLOR_BUFFER_BIT | eGL_STENCIL_BUFFER_BIT))
    {
      name.pop_back();    // ','
      name.pop_back();    // ' '
    }

    name += ")";

    DrawcallDescription draw;
    draw.name = name;
    draw.flags |= DrawFlags::Clear;
    if(Mask & GL_COLOR_BUFFER_BIT)
      draw.flags |= DrawFlags::ClearColor;
    if(Mask & (eGL_DEPTH_BUFFER_BIT | eGL_STENCIL_BUFFER_BIT))
      draw.flags |= DrawFlags::ClearDepthStencil;

    AddDrawcall(draw, true);

    GLuint attachment = 0;
    GLenum type = eGL_TEXTURE;

    if(Mask & GL_DEPTH_BUFFER_BIT)
    {
      m_Real.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                                   eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                   (GLint *)&attachment);
      m_Real.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                                   eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                   (GLint *)&type);

      if(attachment)
      {
        if(type == eGL_TEXTURE)
          m_ResourceUses[GetResourceManager()->GetID(TextureRes(GetCtx(), attachment))].push_back(
              EventUsage(m_CurEventID, ResourceUsage::Clear));
        else
          m_ResourceUses[GetResourceManager()->GetID(RenderbufferRes(GetCtx(), attachment))].push_back(
              EventUsage(m_CurEventID, ResourceUsage::Clear));
      }
    }

    attachment = 0;
    type = eGL_TEXTURE;

    if(Mask & GL_STENCIL_BUFFER_BIT)
    {
      m_Real.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                                   eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                   (GLint *)&attachment);
      m_Real.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                                   eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                   (GLint *)&type);

      if(attachment)
      {
        if(type == eGL_TEXTURE)
          m_ResourceUses[GetResourceManager()->GetID(TextureRes(GetCtx(), attachment))].push_back(
              EventUsage(m_CurEventID, ResourceUsage::Clear));
        else
          m_ResourceUses[GetResourceManager()->GetID(RenderbufferRes(GetCtx(), attachment))].push_back(
              EventUsage(m_CurEventID, ResourceUsage::Clear));
      }
    }

    if(Mask & GL_COLOR_BUFFER_BIT)
    {
      GLint numCols = 8;
      m_Real.glGetIntegerv(eGL_MAX_COLOR_ATTACHMENTS, &numCols);

      for(int i = 0; i < numCols; i++)
      {
        attachment = 0;
        type = eGL_TEXTURE;

        m_Real.glGetFramebufferAttachmentParameteriv(
            eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + i),
            eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&attachment);
        m_Real.glGetFramebufferAttachmentParameteriv(
            eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + i),
            eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

        if(attachment)
        {
          if(type == eGL_TEXTURE)
            m_ResourceUses[GetResourceManager()->GetID(TextureRes(GetCtx(), attachment))].push_back(
                EventUsage(m_CurEventID, ResourceUsage::Clear));
          else
            m_ResourceUses[GetResourceManager()->GetID(RenderbufferRes(GetCtx(), attachment))].push_back(
                EventUsage(m_CurEventID, ResourceUsage::Clear));
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

bool WrappedOpenGL::Serialise_glClearTexImage(GLuint texture, GLint level, GLenum format,
                                              GLenum type, const void *data)
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
      // fall through
      case eGL_RED:
      case eGL_RED_INTEGER:
      case eGL_GREEN_INTEGER:
      case eGL_BLUE_INTEGER:
      case eGL_DEPTH_COMPONENT:
      case eGL_STENCIL_INDEX: s = 1; break;
      case eGL_RG:
      case eGL_RG_INTEGER:
      case eGL_DEPTH_STENCIL: s = 2; break;
      case eGL_RGB:
      case eGL_BGR:
      case eGL_RGB_INTEGER:
      case eGL_BGR_INTEGER: s = 3; break;
      case eGL_RGBA:
      case eGL_BGRA:
      case eGL_RGBA_INTEGER:
      case eGL_BGRA_INTEGER: s = 4; break;
    }
    switch(Type)
    {
      case eGL_UNSIGNED_BYTE:
      case eGL_BYTE: s *= 1; break;
      case eGL_UNSIGNED_SHORT:
      case eGL_SHORT: s *= 2; break;
      case eGL_UNSIGNED_INT:
      case eGL_INT:
      case eGL_FLOAT: s *= 4; break;
      default:
        RDCWARN("Unexpected type %x, defaulting to 1 byte type", Format);
      // fall through
      case eGL_UNSIGNED_BYTE_3_3_2:
      case eGL_UNSIGNED_BYTE_2_3_3_REV: s = 1; break;
      case eGL_UNSIGNED_SHORT_5_6_5:
      case eGL_UNSIGNED_SHORT_5_6_5_REV:
      case eGL_UNSIGNED_SHORT_4_4_4_4:
      case eGL_UNSIGNED_SHORT_4_4_4_4_REV:
      case eGL_UNSIGNED_SHORT_5_5_5_1:
      case eGL_UNSIGNED_SHORT_1_5_5_5_REV:
      case eGL_UNSIGNED_INT_8_8_8_8:
      case eGL_UNSIGNED_INT_8_8_8_8_REV: s = 2; break;
      case eGL_UNSIGNED_INT_10_10_10_2:
      case eGL_UNSIGNED_INT_2_10_10_10_REV: s = 4; break;
    }
    memcpy(val, data, s);
  }

  m_pSerialiser->SerialisePODArray<4>("data", val);

  if(m_State <= EXECUTING)
  {
    m_Real.glClearTexImage(GetResourceManager()->GetLiveResource(id).name, Level, Format, Type,
                           (const void *)&val[0]);
  }

  return true;
}

void WrappedOpenGL::glClearTexImage(GLuint texture, GLint level, GLenum format, GLenum type,
                                    const void *data)
{
  CoherentMapImplicitBarrier();

  m_Real.glClearTexImage(texture, level, format, type, data);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(CLEARTEXIMAGE);
    Serialise_glClearTexImage(texture, level, format, type, data);

    m_ContextRecord->AddChunk(scope.Get());
    m_MissingTracks.insert(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
  }
  else if(m_State == WRITING_IDLE)
  {
    GetResourceManager()->MarkDirtyResource(TextureRes(GetCtx(), texture));
  }
}

bool WrappedOpenGL::Serialise_glClearTexSubImage(GLuint texture, GLint level, GLint xoffset,
                                                 GLint yoffset, GLint zoffset, GLsizei width,
                                                 GLsizei height, GLsizei depth, GLenum format,
                                                 GLenum type, const void *data)
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
      // fall through
      case eGL_RED:
      case eGL_RED_INTEGER:
      case eGL_GREEN_INTEGER:
      case eGL_BLUE_INTEGER:
      case eGL_DEPTH_COMPONENT:
      case eGL_STENCIL_INDEX: s = 1; break;
      case eGL_RG:
      case eGL_RG_INTEGER:
      case eGL_DEPTH_STENCIL: s = 2; break;
      case eGL_RGB:
      case eGL_BGR:
      case eGL_RGB_INTEGER:
      case eGL_BGR_INTEGER: s = 3; break;
      case eGL_RGBA:
      case eGL_BGRA:
      case eGL_RGBA_INTEGER:
      case eGL_BGRA_INTEGER: s = 4; break;
    }
    switch(Type)
    {
      case eGL_UNSIGNED_BYTE:
      case eGL_BYTE: s *= 1; break;
      case eGL_UNSIGNED_SHORT:
      case eGL_SHORT: s *= 2; break;
      case eGL_UNSIGNED_INT:
      case eGL_INT:
      case eGL_FLOAT: s *= 4; break;
      default:
        RDCWARN("Unexpected type %x, defaulting to 1 byte type", Format);
      // fall through
      case eGL_UNSIGNED_BYTE_3_3_2:
      case eGL_UNSIGNED_BYTE_2_3_3_REV: s = 1; break;
      case eGL_UNSIGNED_SHORT_5_6_5:
      case eGL_UNSIGNED_SHORT_5_6_5_REV:
      case eGL_UNSIGNED_SHORT_4_4_4_4:
      case eGL_UNSIGNED_SHORT_4_4_4_4_REV:
      case eGL_UNSIGNED_SHORT_5_5_5_1:
      case eGL_UNSIGNED_SHORT_1_5_5_5_REV:
      case eGL_UNSIGNED_INT_8_8_8_8:
      case eGL_UNSIGNED_INT_8_8_8_8_REV: s = 2; break;
      case eGL_UNSIGNED_INT_10_10_10_2:
      case eGL_UNSIGNED_INT_2_10_10_10_REV: s = 4; break;
    }
    memcpy(val, data, s);
  }

  m_pSerialiser->SerialisePODArray<4>("data", val);

  if(m_State <= EXECUTING)
  {
    m_Real.glClearTexSubImage(GetResourceManager()->GetLiveResource(id).name, Level, Xoffs, Yoffs,
                              Zoffs, w, h, d, Format, Type, (const void *)&val[0]);
  }

  return true;
}

void WrappedOpenGL::glClearTexSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                                       GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                       GLenum format, GLenum type, const void *data)
{
  CoherentMapImplicitBarrier();

  m_Real.glClearTexSubImage(texture, level, xoffset, yoffset, zoffset, width, height, depth, format,
                            type, data);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(CLEARTEXSUBIMAGE);
    Serialise_glClearTexSubImage(texture, level, xoffset, yoffset, zoffset, width, height, depth,
                                 format, type, data);

    m_ContextRecord->AddChunk(scope.Get());
    m_MissingTracks.insert(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
  }
  else if(m_State == WRITING_IDLE)
  {
    GetResourceManager()->MarkDirtyResource(TextureRes(GetCtx(), texture));
  }
}

bool WrappedOpenGL::Serialise_glPrimitiveBoundingBox(GLfloat minX, GLfloat minY, GLfloat minZ,
                                                     GLfloat minW, GLfloat maxX, GLfloat maxY,
                                                     GLfloat maxZ, GLfloat maxW)
{
  SERIALISE_ELEMENT(float, MinX, minX);
  SERIALISE_ELEMENT(float, MinY, minY);
  SERIALISE_ELEMENT(float, MinZ, minZ);
  SERIALISE_ELEMENT(float, MinW, minW);
  SERIALISE_ELEMENT(float, MaxX, maxX);
  SERIALISE_ELEMENT(float, MaxY, maxY);
  SERIALISE_ELEMENT(float, MaxZ, maxZ);
  SERIALISE_ELEMENT(float, MaxW, maxW);

  if(m_State <= EXECUTING)
  {
    m_Real.glPrimitiveBoundingBox(MinX, MinY, MinZ, MinW, MaxX, MaxY, MaxZ, MaxW);
  }

  return true;
}

void WrappedOpenGL::glPrimitiveBoundingBox(GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW,
                                           GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW)
{
  m_Real.glPrimitiveBoundingBox(minX, minY, minZ, minW, maxX, maxY, maxZ, maxW);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(PRIMITIVE_BOUNDING_BOX);
    Serialise_glPrimitiveBoundingBox(minX, minY, minZ, minW, maxX, maxY, maxZ, maxW);
    m_ContextRecord->AddChunk(scope.Get());
  }
}
