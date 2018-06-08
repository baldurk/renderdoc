/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2018 Baldur Karlsson
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
#include "strings/string_utils.h"

enum GLbarrierbitfield
{
};

DECLARE_REFLECTION_ENUM(GLbarrierbitfield);

template <>
std::string DoStringise(const GLbarrierbitfield &el)
{
  RDCCOMPILE_ASSERT(sizeof(GLbarrierbitfield) == sizeof(GLbitfield) &&
                        sizeof(GLbarrierbitfield) == sizeof(uint32_t),
                    "Fake bitfield enum must be uint32_t sized");

  BEGIN_BITFIELD_STRINGISE(GLbarrierbitfield);
  {
    STRINGISE_BITFIELD_VALUE_NAMED((GLbarrierbitfield)GL_ALL_BARRIER_BITS, "GL_ALL_BARRIER_BITS");

    STRINGISE_BITFIELD_BIT(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
    STRINGISE_BITFIELD_BIT(GL_ELEMENT_ARRAY_BARRIER_BIT);
    STRINGISE_BITFIELD_BIT(GL_UNIFORM_BARRIER_BIT);
    STRINGISE_BITFIELD_BIT(GL_TEXTURE_FETCH_BARRIER_BIT);
    STRINGISE_BITFIELD_BIT(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    STRINGISE_BITFIELD_BIT(GL_COMMAND_BARRIER_BIT);
    STRINGISE_BITFIELD_BIT(GL_PIXEL_BUFFER_BARRIER_BIT);
    STRINGISE_BITFIELD_BIT(GL_TEXTURE_UPDATE_BARRIER_BIT);
    STRINGISE_BITFIELD_BIT(GL_BUFFER_UPDATE_BARRIER_BIT);
    STRINGISE_BITFIELD_BIT(GL_FRAMEBUFFER_BARRIER_BIT);
    STRINGISE_BITFIELD_BIT(GL_TRANSFORM_FEEDBACK_BARRIER_BIT);
    STRINGISE_BITFIELD_BIT(GL_ATOMIC_COUNTER_BARRIER_BIT);
    STRINGISE_BITFIELD_BIT(GL_SHADER_STORAGE_BARRIER_BIT);
  }
  END_BITFIELD_STRINGISE();
}

static constexpr uint32_t GetIdxSize(GLenum idxtype)
{
  return (idxtype == eGL_UNSIGNED_BYTE ? 1 : (idxtype == eGL_UNSIGNED_SHORT ? 2 : 4));
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDispatchCompute(SerialiserType &ser, GLuint num_groups_x,
                                                GLuint num_groups_y, GLuint num_groups_z)
{
  SERIALISE_ELEMENT(num_groups_x);
  SERIALISE_ELEMENT(num_groups_y);
  SERIALISE_ELEMENT(num_groups_z);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glDispatchCompute(num_groups_x, num_groups_y, num_groups_z);

    if(IsLoading(m_State))
    {
      AddEvent();

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(%u, %u, %u)", ToStr(gl_CurChunk).c_str(), num_groups_x,
                                    num_groups_y, num_groups_z);
      draw.flags |= DrawFlags::Dispatch;

      draw.dispatchDimension[0] = num_groups_x;
      draw.dispatchDimension[1] = num_groups_y;
      draw.dispatchDimension[2] = num_groups_z;

      if(num_groups_x == 0)
        AddDebugMessage(MessageCategory::Execution, MessageSeverity::Medium,
                        MessageSource::IncorrectAPIUse,
                        "Dispatch call has num_groups_x=0. This will do nothing, which is unusual "
                        "for a non-indirect Dispatch. Did you mean X=1?");
      if(num_groups_x == 0)
        AddDebugMessage(MessageCategory::Execution, MessageSeverity::Medium,
                        MessageSource::IncorrectAPIUse,
                        "Dispatch call has num_groups_x=0. This will do nothing, which is unusual "
                        "for a non-indirect Dispatch. Did you mean Y=1?");
      if(num_groups_z == 0)
        AddDebugMessage(MessageCategory::Execution, MessageSeverity::Medium,
                        MessageSource::IncorrectAPIUse,
                        "Dispatch call has num_groups_z=0. This will do nothing, which is unusual "
                        "for a non-indirect Dispatch. Did you mean Z=1?");

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedOpenGL::glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glDispatchCompute(num_groups_x, num_groups_y, num_groups_z));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDispatchCompute(ser, num_groups_x, num_groups_y, num_groups_z);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDispatchComputeGroupSizeARB(SerialiserType &ser,
                                                            GLuint num_groups_x, GLuint num_groups_y,
                                                            GLuint num_groups_z, GLuint group_size_x,
                                                            GLuint group_size_y, GLuint group_size_z)
{
  SERIALISE_ELEMENT(num_groups_x);
  SERIALISE_ELEMENT(num_groups_y);
  SERIALISE_ELEMENT(num_groups_z);
  SERIALISE_ELEMENT(group_size_x);
  SERIALISE_ELEMENT(group_size_y);
  SERIALISE_ELEMENT(group_size_z);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glDispatchComputeGroupSizeARB(num_groups_x, num_groups_y, num_groups_z, group_size_x,
                                         group_size_y, group_size_z);

    if(IsLoading(m_State))
    {
      AddEvent();

      DrawcallDescription draw;
      draw.name =
          StringFormat::Fmt("%s(%u, %u, %u,  %u, %u, %u)", ToStr(gl_CurChunk).c_str(), num_groups_x,
                            num_groups_y, num_groups_z, group_size_x, group_size_y, group_size_z);
      draw.flags |= DrawFlags::Dispatch;

      draw.dispatchDimension[0] = num_groups_x;
      draw.dispatchDimension[1] = num_groups_y;
      draw.dispatchDimension[2] = num_groups_z;
      draw.dispatchThreadsDimension[0] = group_size_x;
      draw.dispatchThreadsDimension[1] = group_size_y;
      draw.dispatchThreadsDimension[2] = group_size_z;

      if(num_groups_x == 0)
        AddDebugMessage(MessageCategory::Execution, MessageSeverity::Medium,
                        MessageSource::IncorrectAPIUse,
                        "Dispatch call has num_groups_x=0. This will do nothing, which is unusual "
                        "for a non-indirect Dispatch. Did you mean X=1?");
      if(num_groups_y == 0)
        AddDebugMessage(MessageCategory::Execution, MessageSeverity::Medium,
                        MessageSource::IncorrectAPIUse,
                        "Dispatch call has num_groups_y=0. This will do nothing, which is unusual "
                        "for a non-indirect Dispatch. Did you mean Y=1?");
      if(num_groups_z == 0)
        AddDebugMessage(MessageCategory::Execution, MessageSeverity::Medium,
                        MessageSource::IncorrectAPIUse,
                        "Dispatch call has num_groups_z=0. This will do nothing, which is unusual "
                        "for a non-indirect Dispatch. Did you mean Z=1?");

      if(group_size_x == 0)
        AddDebugMessage(MessageCategory::Execution, MessageSeverity::Medium,
                        MessageSource::IncorrectAPIUse,
                        "Dispatch call has group_size_x=0. This will do nothing, which is unusual "
                        "for a non-indirect Dispatch. Did you mean X=1?");
      if(group_size_y == 0)
        AddDebugMessage(MessageCategory::Execution, MessageSeverity::Medium,
                        MessageSource::IncorrectAPIUse,
                        "Dispatch call has group_size_y=0. This will do nothing, which is unusual "
                        "for a non-indirect Dispatch. Did you mean Y=1?");
      if(group_size_z == 0)
        AddDebugMessage(MessageCategory::Execution, MessageSeverity::Medium,
                        MessageSource::IncorrectAPIUse,
                        "Dispatch call has group_size_z=0. This will do nothing, which is unusual "
                        "for a non-indirect Dispatch. Did you mean Z=1?");

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedOpenGL::glDispatchComputeGroupSizeARB(GLuint num_groups_x, GLuint num_groups_y,
                                                  GLuint num_groups_z, GLuint group_size_x,
                                                  GLuint group_size_y, GLuint group_size_z)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glDispatchComputeGroupSizeARB(
      num_groups_x, num_groups_y, num_groups_z, group_size_x, group_size_y, group_size_z));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDispatchComputeGroupSizeARB(ser, num_groups_x, num_groups_y, num_groups_z,
                                            group_size_x, group_size_y, group_size_z);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDispatchComputeIndirect(SerialiserType &ser, GLintptr indirect)
{
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)indirect);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glDispatchComputeIndirect((GLintptr)offset);

    if(IsLoading(m_State))
    {
      uint32_t groupSizes[3];
      m_Real.glGetBufferSubData(eGL_DISPATCH_INDIRECT_BUFFER, (GLintptr)offset,
                                sizeof(uint32_t) * 3, groupSizes);

      AddEvent();

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(<%u, %u, %u>)", ToStr(gl_CurChunk).c_str(), groupSizes[0],
                                    groupSizes[1], groupSizes[2]);
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
  }

  return true;
}

void WrappedOpenGL::glDispatchComputeIndirect(GLintptr indirect)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glDispatchComputeIndirect(indirect));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDispatchComputeIndirect(ser, indirect);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glMemoryBarrier(SerialiserType &ser, GLbitfield barriers)
{
  SERIALISE_ELEMENT_TYPED(GLbarrierbitfield, barriers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glMemoryBarrier(barriers);
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

  SERIALISE_TIME_CALL(m_Real.glMemoryBarrier(barriers));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glMemoryBarrier(ser, barriers);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glMemoryBarrierByRegion(SerialiserType &ser, GLbitfield barriers)
{
  SERIALISE_ELEMENT_TYPED(GLbarrierbitfield, barriers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glMemoryBarrierByRegion(barriers);
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

  SERIALISE_TIME_CALL(m_Real.glMemoryBarrierByRegion(barriers));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glMemoryBarrierByRegion(ser, barriers);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureBarrier(SerialiserType &ser)
{
  if(IsReplayingAndReading())
  {
    m_Real.glTextureBarrier();
  }

  return true;
}

void WrappedOpenGL::glTextureBarrier()
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glTextureBarrier());

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTextureBarrier(ser);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDrawTransformFeedback(SerialiserType &ser, GLenum mode,
                                                      GLuint xfbHandle)
{
  SERIALISE_ELEMENT(mode);
  SERIALISE_ELEMENT_LOCAL(xfb, FeedbackRes(GetCtx(), xfbHandle));

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glDrawTransformFeedback(mode, xfb.name);

    if(IsLoading(m_State))
    {
      AddEvent();

      GLNOTIMP("Not fetching feedback object count for glDrawTransformFeedback() display");

      DrawcallDescription draw;
      draw.name = ToStr(gl_CurChunk) + "(<?>)";
      draw.numIndices = 1;
      draw.numInstances = 1;
      draw.indexOffset = 0;
      draw.vertexOffset = 0;
      draw.instanceOffset = 0;

      draw.flags |= DrawFlags::Drawcall;

      draw.topology = MakePrimitiveTopology(m_Real, mode);

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedOpenGL::glDrawTransformFeedback(GLenum mode, GLuint id)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glDrawTransformFeedback(mode, id));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDrawTransformFeedback(ser, mode, id);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDrawTransformFeedbackInstanced(SerialiserType &ser, GLenum mode,
                                                               GLuint xfbHandle,
                                                               GLsizei instancecount)
{
  SERIALISE_ELEMENT(mode);
  SERIALISE_ELEMENT_LOCAL(xfb, FeedbackRes(GetCtx(), xfbHandle));
  SERIALISE_ELEMENT(instancecount);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glDrawTransformFeedbackInstanced(mode, xfb.name, instancecount);

    if(IsLoading(m_State))
    {
      AddEvent();

      GLNOTIMP("Not fetching feedback object count for glDrawTransformFeedbackInstanced() display");

      DrawcallDescription draw;
      draw.name = ToStr(gl_CurChunk) + "(<?>)";
      draw.numIndices = 1;
      draw.numInstances = 1;
      draw.indexOffset = 0;
      draw.vertexOffset = 0;
      draw.instanceOffset = 0;

      draw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced;

      draw.topology = MakePrimitiveTopology(m_Real, mode);

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedOpenGL::glDrawTransformFeedbackInstanced(GLenum mode, GLuint id, GLsizei instancecount)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glDrawTransformFeedbackInstanced(mode, id, instancecount));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDrawTransformFeedbackInstanced(ser, mode, id, instancecount);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDrawTransformFeedbackStream(SerialiserType &ser, GLenum mode,
                                                            GLuint xfbHandle, GLuint stream)
{
  SERIALISE_ELEMENT(mode);
  SERIALISE_ELEMENT_LOCAL(xfb, FeedbackRes(GetCtx(), xfbHandle));
  SERIALISE_ELEMENT(stream);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glDrawTransformFeedbackStream(mode, xfb.name, stream);

    if(IsLoading(m_State))
    {
      AddEvent();

      GLNOTIMP("Not fetching feedback object count for glDrawTransformFeedbackStream() display");

      DrawcallDescription draw;
      draw.name = ToStr(gl_CurChunk) + "(<?>)";
      draw.numIndices = 1;
      draw.numInstances = 1;
      draw.indexOffset = 0;
      draw.vertexOffset = 0;
      draw.instanceOffset = 0;

      draw.flags |= DrawFlags::Drawcall;

      draw.topology = MakePrimitiveTopology(m_Real, mode);

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedOpenGL::glDrawTransformFeedbackStream(GLenum mode, GLuint id, GLuint stream)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glDrawTransformFeedbackStream(mode, id, stream));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDrawTransformFeedbackStream(ser, mode, id, stream);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDrawTransformFeedbackStreamInstanced(SerialiserType &ser, GLenum mode,
                                                                     GLuint xfbHandle, GLuint stream,
                                                                     GLsizei instancecount)
{
  SERIALISE_ELEMENT(mode);
  SERIALISE_ELEMENT_LOCAL(xfb, FeedbackRes(GetCtx(), xfbHandle));
  SERIALISE_ELEMENT(stream);
  SERIALISE_ELEMENT(instancecount);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glDrawTransformFeedbackStreamInstanced(mode, xfb.name, stream, instancecount);

    if(IsLoading(m_State))
    {
      AddEvent();

      GLNOTIMP(
          "Not fetching feedback object count for glDrawTransformFeedbackStreamInstanced() "
          "display");

      DrawcallDescription draw;
      draw.name = ToStr(gl_CurChunk) + "(<?>)";
      draw.numIndices = 1;
      draw.numInstances = 1;
      draw.indexOffset = 0;
      draw.vertexOffset = 0;
      draw.instanceOffset = 0;

      draw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced;

      draw.topology = MakePrimitiveTopology(m_Real, mode);

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedOpenGL::glDrawTransformFeedbackStreamInstanced(GLenum mode, GLuint id, GLuint stream,
                                                           GLsizei instancecount)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glDrawTransformFeedbackStreamInstanced(mode, id, stream, instancecount));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDrawTransformFeedbackStreamInstanced(ser, mode, id, stream, instancecount);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDrawArrays(SerialiserType &ser, GLenum mode, GLint first,
                                           GLsizei count)
{
  SERIALISE_ELEMENT(mode);
  SERIALISE_ELEMENT(first);
  SERIALISE_ELEMENT(count);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glDrawArrays(mode, first, count);

    if(IsLoading(m_State))
    {
      AddEvent();

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(%u)", ToStr(gl_CurChunk).c_str(), count);
      draw.numIndices = count;
      draw.numInstances = 1;
      draw.indexOffset = 0;
      draw.vertexOffset = first;
      draw.instanceOffset = 0;

      draw.flags |= DrawFlags::Drawcall;

      draw.topology = MakePrimitiveTopology(m_Real, mode);

      AddDrawcall(draw, true);
    }
  }

  return true;
}

WrappedOpenGL::ClientMemoryData *WrappedOpenGL::CopyClientMemoryArrays(GLint first, GLsizei count,
                                                                       GLenum indexType,
                                                                       const void *&indices)
{
  PUSH_CURRENT_CHUNK;
  RDCASSERT(IsActiveCapturing(m_State));
  ContextData &cd = GetCtxData();

  GLint idxbuf = 0;
  GLsizeiptr idxlen = 0;
  const void *mmIndices = indices;
  if(indexType != eGL_NONE)
  {
    idxlen = GLsizeiptr(count) * GetIdxSize(indexType);

    m_Real.glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, &idxbuf);
    if(idxbuf == 0)
    {
      // Bind and update fake index buffer, to draw from the 'immediate' index data
      gl_CurChunk = GLChunk::glBindBuffer;
      glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, cd.m_ClientMemoryIBO);

      gl_CurChunk = GLChunk::glBufferData;
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

    gl_CurChunk = GLChunk::glBindBuffer;
    glBindBuffer(eGL_ARRAY_BUFFER, cd.m_ClientMemoryVBOs[i]);

    // Copy all client memory, and the pointer becomes a zero offset.
    gl_CurChunk = GLChunk::glBufferData;
    glBufferData(eGL_ARRAY_BUFFER, (first + count) * totalStride, attrib.pointer, eGL_STATIC_DRAW);

    gl_CurChunk = GLChunk::glVertexAttribPointer;
    glVertexAttribPointer(attrib.index, attrib.size, attrib.type, attrib.normalized, attrib.stride,
                          NULL);

    clientMemory->attribs.push_back(attrib);
  }

  return clientMemory;
}

void WrappedOpenGL::RestoreClientMemoryArrays(ClientMemoryData *clientMemoryArrays, GLenum indexType)
{
  PUSH_CURRENT_CHUNK;

  if(indexType != eGL_NONE)
  {
    ContextData &cd = GetCtxData();
    GLuint idxbuf = 0;
    m_Real.glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, (GLint *)&idxbuf);
    if(idxbuf == cd.m_ClientMemoryIBO)
    {
      // Restore the zero buffer binding if we were using the fake index buffer.
      gl_CurChunk = GLChunk::glBindBuffer;
      glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, 0);
    }
  }

  if(!clientMemoryArrays)
    return;

  // Restore the 0-buffer bindings and attrib pointers.
  gl_CurChunk = GLChunk::glBindBuffer;
  glBindBuffer(eGL_ARRAY_BUFFER, 0);

  for(const ClientMemoryData::VertexAttrib &attrib : clientMemoryArrays->attribs)
  {
    gl_CurChunk = GLChunk::glVertexAttribPointer;
    glVertexAttribPointer(attrib.index, attrib.size, attrib.type, attrib.normalized, attrib.stride,
                          attrib.pointer);
  }

  gl_CurChunk = GLChunk::glBindBuffer;
  glBindBuffer(eGL_ARRAY_BUFFER, clientMemoryArrays->prevArrayBufferBinding);

  delete clientMemoryArrays;
}

void WrappedOpenGL::glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glDrawArrays(mode, first, count));

  if(IsActiveCapturing(m_State))
  {
    const void *indices = NULL;
    ClientMemoryData *clientMemory = CopyClientMemoryArrays(first, count, eGL_NONE, indices);

    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDrawArrays(ser, mode, first, count);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);

    RestoreClientMemoryArrays(clientMemory, eGL_NONE);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDrawArraysIndirect(SerialiserType &ser, GLenum mode,
                                                   const void *indirect)
{
  SERIALISE_ELEMENT(mode);
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)indirect);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glDrawArraysIndirect(mode, (const void *)offset);

    if(IsLoading(m_State))
    {
      DrawArraysIndirectCommand params;
      m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, (GLintptr)offset, sizeof(params), &params);

      AddEvent();

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(%u, %u)", ToStr(gl_CurChunk).c_str(), params.count,
                                    params.instanceCount);
      draw.numIndices = params.count;
      draw.numInstances = params.instanceCount;
      draw.vertexOffset = params.first;
      draw.instanceOffset = params.baseInstance;

      draw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::Indirect;

      draw.topology = MakePrimitiveTopology(m_Real, mode);

      AddDrawcall(draw, true);

      GLuint buf = 0;
      m_Real.glGetIntegerv(eGL_DRAW_INDIRECT_BUFFER_BINDING, (GLint *)&buf);

      m_ResourceUses[GetResourceManager()->GetID(BufferRes(GetCtx(), buf))].push_back(
          EventUsage(m_CurEventID, ResourceUsage::Indirect));
    }
  }

  return true;
}

void WrappedOpenGL::glDrawArraysIndirect(GLenum mode, const void *indirect)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glDrawArraysIndirect(mode, indirect));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDrawArraysIndirect(ser, mode, indirect);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDrawArraysInstanced(SerialiserType &ser, GLenum mode, GLint first,
                                                    GLsizei count, GLsizei instancecount)
{
  SERIALISE_ELEMENT(mode);
  SERIALISE_ELEMENT(first);
  SERIALISE_ELEMENT(count);
  SERIALISE_ELEMENT(instancecount);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glDrawArraysInstanced(mode, first, count, instancecount);

    if(IsLoading(m_State))
    {
      AddEvent();

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(%u, %u)", ToStr(gl_CurChunk).c_str(), count, instancecount);
      draw.numIndices = count;
      draw.numInstances = instancecount;
      draw.indexOffset = 0;
      draw.vertexOffset = first;
      draw.instanceOffset = 0;

      draw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced;

      draw.topology = MakePrimitiveTopology(m_Real, mode);

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedOpenGL::glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count,
                                          GLsizei instancecount)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glDrawArraysInstanced(mode, first, count, instancecount));

  if(IsActiveCapturing(m_State))
  {
    const void *indices = NULL;
    ClientMemoryData *clientMemory = CopyClientMemoryArrays(first, count, eGL_NONE, indices);

    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDrawArraysInstanced(ser, mode, first, count, instancecount);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);

    RestoreClientMemoryArrays(clientMemory, eGL_NONE);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDrawArraysInstancedBaseInstance(SerialiserType &ser, GLenum mode,
                                                                GLint first, GLsizei count,
                                                                GLsizei instancecount,
                                                                GLuint baseinstance)
{
  SERIALISE_ELEMENT(mode);
  SERIALISE_ELEMENT(first);
  SERIALISE_ELEMENT(count);
  SERIALISE_ELEMENT(instancecount);
  SERIALISE_ELEMENT(baseinstance);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glDrawArraysInstancedBaseInstance(mode, first, count, instancecount, baseinstance);

    if(IsLoading(m_State))
    {
      AddEvent();

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(%u, %u)", ToStr(gl_CurChunk).c_str(), count, instancecount);
      draw.numIndices = count;
      draw.numInstances = instancecount;
      draw.indexOffset = 0;
      draw.vertexOffset = first;
      draw.instanceOffset = baseinstance;

      draw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced;

      draw.topology = MakePrimitiveTopology(m_Real, mode);

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedOpenGL::glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count,
                                                      GLsizei instancecount, GLuint baseinstance)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(
      m_Real.glDrawArraysInstancedBaseInstance(mode, first, count, instancecount, baseinstance));

  if(IsActiveCapturing(m_State))
  {
    const void *indices = NULL;
    ClientMemoryData *clientMemory = CopyClientMemoryArrays(first, count, eGL_NONE, indices);

    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDrawArraysInstancedBaseInstance(ser, mode, first, count, instancecount, baseinstance);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);

    RestoreClientMemoryArrays(clientMemory, eGL_NONE);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
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

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDrawElements(SerialiserType &ser, GLenum mode, GLsizei count,
                                             GLenum type, const void *indicesPtr)
{
  SERIALISE_ELEMENT(mode);
  SERIALISE_ELEMENT(count);
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT_LOCAL(indices, (uint64_t)indicesPtr);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(Check_preElements())
      m_Real.glDrawElements(mode, count, type, (const void *)indices);

    if(IsLoading(m_State))
    {
      AddEvent();

      uint32_t IdxSize = GetIdxSize(type);

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(%u)", ToStr(gl_CurChunk).c_str(), count);
      draw.numIndices = count;
      draw.numInstances = 1;
      draw.indexOffset = uint32_t(indices) / IdxSize;
      draw.vertexOffset = 0;
      draw.instanceOffset = 0;

      draw.flags |= DrawFlags::Drawcall | DrawFlags::UseIBuffer;

      draw.topology = MakePrimitiveTopology(m_Real, mode);
      draw.indexByteWidth = IdxSize;

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedOpenGL::glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glDrawElements(mode, count, type, indices));

  if(IsActiveCapturing(m_State))
  {
    ClientMemoryData *clientMemory = CopyClientMemoryArrays(-1, count, type, indices);

    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDrawElements(ser, mode, count, type, indices);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);

    RestoreClientMemoryArrays(clientMemory, type);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDrawElementsIndirect(SerialiserType &ser, GLenum mode, GLenum type,
                                                     const void *indirect)
{
  SERIALISE_ELEMENT(mode);
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)indirect);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glDrawElementsIndirect(mode, type, (const void *)offset);

    if(IsLoading(m_State))
    {
      DrawElementsIndirectCommand params;
      m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, (GLintptr)offset, sizeof(params), &params);

      AddEvent();

      uint32_t IdxSize = GetIdxSize(type);

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(<%u, %u>)", ToStr(gl_CurChunk).c_str(), params.count,
                                    params.instanceCount);
      draw.numIndices = params.count;
      draw.numInstances = params.instanceCount;
      draw.indexOffset = params.firstIndex;
      draw.baseVertex = params.baseVertex;
      draw.instanceOffset = params.baseInstance;

      draw.flags |=
          DrawFlags::Drawcall | DrawFlags::UseIBuffer | DrawFlags::Instanced | DrawFlags::Indirect;

      draw.topology = MakePrimitiveTopology(m_Real, mode);
      draw.indexByteWidth = IdxSize;

      AddDrawcall(draw, true);

      GLuint buf = 0;
      m_Real.glGetIntegerv(eGL_DRAW_INDIRECT_BUFFER_BINDING, (GLint *)&buf);

      m_ResourceUses[GetResourceManager()->GetID(BufferRes(GetCtx(), buf))].push_back(
          EventUsage(m_CurEventID, ResourceUsage::Indirect));
    }
  }

  return true;
}

void WrappedOpenGL::glDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glDrawElementsIndirect(mode, type, indirect));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDrawElementsIndirect(ser, mode, type, indirect);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDrawRangeElements(SerialiserType &ser, GLenum mode, GLuint start,
                                                  GLuint end, GLsizei count, GLenum type,
                                                  const void *indicesPtr)
{
  SERIALISE_ELEMENT(mode);
  SERIALISE_ELEMENT(start);
  SERIALISE_ELEMENT(end);
  SERIALISE_ELEMENT(count);
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT_LOCAL(indices, (uint64_t)indicesPtr);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(Check_preElements())
      m_Real.glDrawRangeElements(mode, start, end, count, type, (const void *)indices);

    if(IsLoading(m_State))
    {
      AddEvent();

      uint32_t IdxSize = GetIdxSize(type);

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(%u)", ToStr(gl_CurChunk).c_str(), count);
      draw.numIndices = count;
      draw.numInstances = 1;
      draw.indexOffset = uint32_t(indices) / IdxSize;
      draw.vertexOffset = 0;
      draw.instanceOffset = 0;

      draw.flags |= DrawFlags::Drawcall | DrawFlags::UseIBuffer;

      draw.topology = MakePrimitiveTopology(m_Real, mode);
      draw.indexByteWidth = IdxSize;

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedOpenGL::glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count,
                                        GLenum type, const void *indices)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glDrawRangeElements(mode, start, end, count, type, indices));

  if(IsActiveCapturing(m_State))
  {
    ClientMemoryData *clientMemory = CopyClientMemoryArrays(-1, count, type, indices);

    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDrawRangeElements(ser, mode, start, end, count, type, indices);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);

    RestoreClientMemoryArrays(clientMemory, type);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDrawRangeElementsBaseVertex(SerialiserType &ser, GLenum mode,
                                                            GLuint start, GLuint end, GLsizei count,
                                                            GLenum type, const void *indicesPtr,
                                                            GLint basevertex)
{
  SERIALISE_ELEMENT(mode);
  SERIALISE_ELEMENT(start);
  SERIALISE_ELEMENT(end);
  SERIALISE_ELEMENT(count);
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT_LOCAL(indices, (uint64_t)indicesPtr);
  SERIALISE_ELEMENT(basevertex);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(Check_preElements())
      m_Real.glDrawRangeElementsBaseVertex(mode, start, end, count, type, (const void *)indices,
                                           basevertex);

    if(IsLoading(m_State))
    {
      AddEvent();

      uint32_t IdxSize = GetIdxSize(type);

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(%u)", ToStr(gl_CurChunk).c_str(), count);
      draw.numIndices = count;
      draw.numInstances = 1;
      draw.indexOffset = uint32_t(indices) / IdxSize;
      draw.baseVertex = basevertex;
      draw.instanceOffset = 0;

      draw.flags |= DrawFlags::Drawcall | DrawFlags::UseIBuffer;

      draw.topology = MakePrimitiveTopology(m_Real, mode);
      draw.indexByteWidth = IdxSize;

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedOpenGL::glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end,
                                                  GLsizei count, GLenum type, const void *indices,
                                                  GLint basevertex)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(
      m_Real.glDrawRangeElementsBaseVertex(mode, start, end, count, type, indices, basevertex));

  if(IsActiveCapturing(m_State))
  {
    ClientMemoryData *clientMemory = CopyClientMemoryArrays(-1, count, type, indices);

    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDrawRangeElementsBaseVertex(ser, mode, start, end, count, type, indices, basevertex);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);

    RestoreClientMemoryArrays(clientMemory, type);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDrawElementsBaseVertex(SerialiserType &ser, GLenum mode,
                                                       GLsizei count, GLenum type,
                                                       const void *indicesPtr, GLint basevertex)
{
  SERIALISE_ELEMENT(mode);
  SERIALISE_ELEMENT(count);
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT_LOCAL(indices, (uint64_t)indicesPtr);
  SERIALISE_ELEMENT(basevertex);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(Check_preElements())
      m_Real.glDrawElementsBaseVertex(mode, count, type, (const void *)indices, basevertex);

    if(IsLoading(m_State))
    {
      AddEvent();

      uint32_t IdxSize = GetIdxSize(type);

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(%u)", ToStr(gl_CurChunk).c_str(), count);
      draw.numIndices = count;
      draw.numInstances = 1;
      draw.indexOffset = uint32_t(indices) / IdxSize;
      draw.baseVertex = basevertex;
      draw.instanceOffset = 0;

      draw.flags |= DrawFlags::Drawcall | DrawFlags::UseIBuffer;

      draw.topology = MakePrimitiveTopology(m_Real, mode);
      draw.indexByteWidth = IdxSize;

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedOpenGL::glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type,
                                             const void *indices, GLint basevertex)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glDrawElementsBaseVertex(mode, count, type, indices, basevertex));

  if(IsActiveCapturing(m_State))
  {
    ClientMemoryData *clientMemory = CopyClientMemoryArrays(-1, count, type, indices);

    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDrawElementsBaseVertex(ser, mode, count, type, indices, basevertex);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);

    RestoreClientMemoryArrays(clientMemory, type);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDrawElementsInstanced(SerialiserType &ser, GLenum mode,
                                                      GLsizei count, GLenum type,
                                                      const void *indicesPtr, GLsizei instancecount)
{
  SERIALISE_ELEMENT(mode);
  SERIALISE_ELEMENT(count);
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT_LOCAL(indices, (uint64_t)indicesPtr);
  SERIALISE_ELEMENT(instancecount);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(Check_preElements())
      m_Real.glDrawElementsInstanced(mode, count, type, (const void *)indices, instancecount);

    if(IsLoading(m_State))
    {
      AddEvent();

      uint32_t IdxSize = GetIdxSize(type);

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(%u, %u)", ToStr(gl_CurChunk).c_str(), count, instancecount);
      draw.numIndices = count;
      draw.numInstances = instancecount;
      draw.indexOffset = uint32_t(indices) / IdxSize;
      draw.vertexOffset = 0;
      draw.instanceOffset = 0;

      draw.flags |= DrawFlags::Drawcall | DrawFlags::UseIBuffer | DrawFlags::Instanced;

      draw.topology = MakePrimitiveTopology(m_Real, mode);
      draw.indexByteWidth = IdxSize;

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedOpenGL::glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type,
                                            const void *indices, GLsizei instancecount)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glDrawElementsInstanced(mode, count, type, indices, instancecount));

  if(IsActiveCapturing(m_State))
  {
    ClientMemoryData *clientMemory = CopyClientMemoryArrays(-1, count, type, indices);

    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDrawElementsInstanced(ser, mode, count, type, indices, instancecount);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);

    RestoreClientMemoryArrays(clientMemory, type);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDrawElementsInstancedBaseInstance(SerialiserType &ser, GLenum mode,
                                                                  GLsizei count, GLenum type,
                                                                  const void *indicesPtr,
                                                                  GLsizei instancecount,
                                                                  GLuint baseinstance)
{
  SERIALISE_ELEMENT(mode);
  SERIALISE_ELEMENT(count);
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT_LOCAL(indices, (uint64_t)indicesPtr);
  SERIALISE_ELEMENT(instancecount);
  SERIALISE_ELEMENT(baseinstance);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(Check_preElements())
      m_Real.glDrawElementsInstancedBaseInstance(mode, count, type, (const void *)indices,
                                                 instancecount, baseinstance);

    if(IsLoading(m_State))
    {
      AddEvent();

      uint32_t IdxSize = GetIdxSize(type);

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(%u, %u)", ToStr(gl_CurChunk).c_str(), count, instancecount);
      draw.numIndices = count;
      draw.numInstances = instancecount;
      draw.indexOffset = uint32_t(indices) / IdxSize;
      draw.vertexOffset = 0;
      draw.instanceOffset = baseinstance;

      draw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::UseIBuffer;

      draw.topology = MakePrimitiveTopology(m_Real, mode);
      draw.indexByteWidth = IdxSize;

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedOpenGL::glDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count, GLenum type,
                                                        const void *indices, GLsizei instancecount,
                                                        GLuint baseinstance)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glDrawElementsInstancedBaseInstance(mode, count, type, indices,
                                                                 instancecount, baseinstance));

  if(IsActiveCapturing(m_State))
  {
    ClientMemoryData *clientMemory = CopyClientMemoryArrays(-1, count, type, indices);

    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDrawElementsInstancedBaseInstance(ser, mode, count, type, indices, instancecount,
                                                  baseinstance);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);

    RestoreClientMemoryArrays(clientMemory, type);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDrawElementsInstancedBaseVertex(SerialiserType &ser, GLenum mode,
                                                                GLsizei count, GLenum type,
                                                                const void *indicesPtr,
                                                                GLsizei instancecount,
                                                                GLint basevertex)
{
  SERIALISE_ELEMENT(mode);
  SERIALISE_ELEMENT(count);
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT_LOCAL(indices, (uint64_t)indicesPtr);
  SERIALISE_ELEMENT(instancecount);
  SERIALISE_ELEMENT(basevertex);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(Check_preElements())
      m_Real.glDrawElementsInstancedBaseVertex(mode, count, type, (const void *)indices,
                                               instancecount, basevertex);

    if(IsLoading(m_State))
    {
      AddEvent();

      uint32_t IdxSize = GetIdxSize(type);

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(%u, %u)", ToStr(gl_CurChunk).c_str(), count, instancecount);
      draw.numIndices = count;
      draw.numInstances = instancecount;
      draw.indexOffset = uint32_t(indices) / IdxSize;
      draw.baseVertex = basevertex;
      draw.instanceOffset = 0;

      draw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::UseIBuffer;

      draw.topology = MakePrimitiveTopology(m_Real, mode);
      draw.indexByteWidth = IdxSize;

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedOpenGL::glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type,
                                                      const void *indices, GLsizei instancecount,
                                                      GLint basevertex)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glDrawElementsInstancedBaseVertex(mode, count, type, indices,
                                                               instancecount, basevertex));

  if(IsActiveCapturing(m_State))
  {
    ClientMemoryData *clientMemory = CopyClientMemoryArrays(-1, count, type, indices);

    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDrawElementsInstancedBaseVertex(ser, mode, count, type, indices, instancecount,
                                                basevertex);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);

    RestoreClientMemoryArrays(clientMemory, type);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDrawElementsInstancedBaseVertexBaseInstance(
    SerialiserType &ser, GLenum mode, GLsizei count, GLenum type, const void *indicesPtr,
    GLsizei instancecount, GLint basevertex, GLuint baseinstance)
{
  SERIALISE_ELEMENT(mode);
  SERIALISE_ELEMENT(count);
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT_LOCAL(indices, (uint64_t)indicesPtr);
  SERIALISE_ELEMENT(instancecount);
  SERIALISE_ELEMENT(basevertex);
  SERIALISE_ELEMENT(baseinstance);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(Check_preElements())
      m_Real.glDrawElementsInstancedBaseVertexBaseInstance(mode, count, type, (const void *)indices,
                                                           instancecount, basevertex, baseinstance);

    if(IsLoading(m_State))
    {
      AddEvent();

      uint32_t IdxSize = GetIdxSize(type);

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(%u, %u)", ToStr(gl_CurChunk).c_str(), count, instancecount);
      draw.numIndices = count;
      draw.numInstances = instancecount;
      draw.indexOffset = uint32_t(indices) / IdxSize;
      draw.baseVertex = basevertex;
      draw.instanceOffset = baseinstance;

      draw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::UseIBuffer;

      draw.topology = MakePrimitiveTopology(m_Real, mode);
      draw.indexByteWidth = IdxSize;

      AddDrawcall(draw, true);
    }
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

  SERIALISE_TIME_CALL(m_Real.glDrawElementsInstancedBaseVertexBaseInstance(
      mode, count, type, indices, instancecount, basevertex, baseinstance));

  if(IsActiveCapturing(m_State))
  {
    ClientMemoryData *clientMemory = CopyClientMemoryArrays(-1, count, type, indices);

    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDrawElementsInstancedBaseVertexBaseInstance(
        ser, mode, count, type, indices, instancecount, basevertex, baseinstance);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);

    RestoreClientMemoryArrays(clientMemory, type);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glMultiDrawArrays(SerialiserType &ser, GLenum mode, const GLint *first,
                                                const GLsizei *count, GLsizei drawcount)
{
  SERIALISE_ELEMENT(mode);
  SERIALISE_ELEMENT_ARRAY(first, drawcount);
  SERIALISE_ELEMENT_ARRAY(count, drawcount);
  SERIALISE_ELEMENT(drawcount);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
    {
      m_Real.glMultiDrawArrays(mode, first, count, drawcount);

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(%i)", ToStr(gl_CurChunk).c_str(), drawcount);
      draw.flags |= DrawFlags::MultiDraw;

      draw.topology = MakePrimitiveTopology(m_Real, mode);

      AddDrawcall(draw, false);

      m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());

      for(GLsizei i = 0; i < drawcount; i++)
      {
        m_CurEventID++;

        DrawcallDescription multidraw;
        multidraw.drawIndex = i;
        multidraw.numIndices = count[i];
        multidraw.vertexOffset = first[i];

        multidraw.name =
            StringFormat::Fmt("%s[%i](%u)", ToStr(gl_CurChunk).c_str(), i, multidraw.numIndices);

        multidraw.flags |= DrawFlags::Drawcall;

        multidraw.topology = MakePrimitiveTopology(m_Real, mode);

        AddEvent();
        AddDrawcall(multidraw, true);
      }

      m_DrawcallStack.pop_back();
    }
    else if(IsActiveReplaying(m_State))
    {
      size_t i = 0;
      for(; i < m_Events.size(); i++)
      {
        if(m_Events[i].eventId >= m_CurEventID)
          break;
      }

      while(i > 1 && m_Events[i - 1].fileOffset == m_Events[i].fileOffset)
        i--;

      uint32_t baseEventID = m_Events[i].eventId;

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
        // by just reducing the drawcount parameter to however many we want to replay. This only
        // works if we're replaying from the first multidraw to the nth (n less than drawcount)
        m_Real.glMultiDrawArrays(mode, first, count,
                                 RDCMIN((uint32_t)drawcount, m_LastEventID - baseEventID + 1));
      }
      else
      {
        // otherwise we do the 'hard' case, draw only one multidraw
        // note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
        // a single draw.
        RDCASSERT(m_LastEventID == m_FirstEventID);

        uint32_t drawidx = (m_LastEventID - baseEventID);

        m_Real.glDrawArrays(mode, first[drawidx], count[drawidx]);
      }

      m_CurEventID += (uint32_t)drawcount;
    }
  }

  return true;
}

void WrappedOpenGL::glMultiDrawArrays(GLenum mode, const GLint *first, const GLsizei *count,
                                      GLsizei drawcount)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glMultiDrawArrays(mode, first, count, drawcount));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glMultiDrawArrays(ser, mode, first, count, drawcount);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glMultiDrawElements(SerialiserType &ser, GLenum mode,
                                                  const GLsizei *count, GLenum type,
                                                  const void *const *indicesPtr, GLsizei drawcount)
{
  // need to serialise the array by hand since the pointers are really offsets :(.
  std::vector<uint64_t> indices;
  if(ser.IsWriting())
  {
    indices.reserve(drawcount);
    for(GLsizei i = 0; i < drawcount; i++)
      indices.push_back((uint64_t)indicesPtr[i]);
  }

  SERIALISE_ELEMENT(mode);
  SERIALISE_ELEMENT_ARRAY(count, drawcount);
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT(indices);
  SERIALISE_ELEMENT(drawcount);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    std::vector<const void *> inds;
    inds.reserve(drawcount);
    for(GLsizei i = 0; i < drawcount; i++)
      inds.push_back((const void *)indices[i]);

    if(IsLoading(m_State))
    {
      m_Real.glMultiDrawElements(mode, count, type, inds.data(), drawcount);

      uint32_t IdxSize = GetIdxSize(type);

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(%i)", ToStr(gl_CurChunk).c_str(), drawcount);

      draw.flags |= DrawFlags::MultiDraw;
      draw.indexByteWidth = IdxSize;
      draw.numIndices = 0;

      draw.topology = MakePrimitiveTopology(m_Real, mode);

      AddDrawcall(draw, false);

      m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());

      for(GLsizei i = 0; i < drawcount; i++)
      {
        m_CurEventID++;

        DrawcallDescription multidraw;
        multidraw.drawIndex = i;
        multidraw.numIndices = count[i];
        multidraw.indexOffset = (uint32_t)(indices[i] & 0xFFFFFFFF);
        multidraw.indexByteWidth = IdxSize;

        multidraw.indexOffset /= IdxSize;

        multidraw.name =
            StringFormat::Fmt("%s[%i](%u)", ToStr(gl_CurChunk).c_str(), i, multidraw.numIndices);

        multidraw.flags |= DrawFlags::Drawcall | DrawFlags::UseIBuffer;

        multidraw.topology = MakePrimitiveTopology(m_Real, mode);

        AddEvent();
        AddDrawcall(multidraw, true);
      }

      m_DrawcallStack.pop_back();
    }
    else if(IsActiveReplaying(m_State))
    {
      size_t i = 0;
      for(; i < m_Events.size(); i++)
      {
        if(m_Events[i].eventId >= m_CurEventID)
          break;
      }

      while(i > 1 && m_Events[i - 1].fileOffset == m_Events[i].fileOffset)
        i--;

      uint32_t baseEventID = m_Events[i].eventId;

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
        m_Real.glMultiDrawElements(mode, count, type, inds.data(),
                                   RDCMIN((uint32_t)drawcount, m_LastEventID - baseEventID + 1));
      }
      else
      {
        // otherwise we do the 'hard' case, draw only one multidraw
        // note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
        // a single draw.
        RDCASSERT(m_LastEventID == m_FirstEventID);

        uint32_t drawidx = (m_LastEventID - baseEventID);

        m_Real.glDrawElements(mode, count[drawidx], type, inds[drawidx]);
      }

      m_CurEventID += (uint32_t)drawcount;
    }
  }

  return true;
}

void WrappedOpenGL::glMultiDrawElements(GLenum mode, const GLsizei *count, GLenum type,
                                        const void *const *indices, GLsizei drawcount)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glMultiDrawElements(mode, count, type, indices, drawcount));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glMultiDrawElements(ser, mode, count, type, indices, drawcount);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glMultiDrawElementsBaseVertex(SerialiserType &ser, GLenum mode,
                                                            const GLsizei *count, GLenum type,
                                                            const void *const *indicesPtr,
                                                            GLsizei drawcount,
                                                            const GLint *basevertex)
{
  // need to serialise the array by hand since the pointers are really offsets :(.
  std::vector<uint64_t> indices;
  if(ser.IsWriting())
  {
    indices.reserve(drawcount);
    for(GLsizei i = 0; i < drawcount; i++)
      indices.push_back((uint64_t)indicesPtr[i]);
  }

  SERIALISE_ELEMENT(mode);
  SERIALISE_ELEMENT_ARRAY(count, drawcount);
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT(indices);
  SERIALISE_ELEMENT(drawcount);
  SERIALISE_ELEMENT_ARRAY(basevertex, drawcount);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    std::vector<const void *> inds;
    inds.reserve(drawcount);
    for(GLsizei i = 0; i < drawcount; i++)
      inds.push_back((const void *)indices[i]);

    if(IsLoading(m_State))
    {
      m_Real.glMultiDrawElementsBaseVertex(mode, count, type, inds.data(), drawcount, basevertex);

      uint32_t IdxSize = GetIdxSize(type);

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(%i)", ToStr(gl_CurChunk).c_str(), drawcount);

      draw.flags |= DrawFlags::MultiDraw;

      draw.topology = MakePrimitiveTopology(m_Real, mode);
      draw.indexByteWidth = IdxSize;

      AddDrawcall(draw, false);

      m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());

      for(GLsizei i = 0; i < drawcount; i++)
      {
        m_CurEventID++;

        DrawcallDescription multidraw;
        multidraw.drawIndex = i;
        multidraw.numIndices = count[i];
        multidraw.indexOffset = (uint32_t)(indices[i] & 0xFFFFFFFF);
        multidraw.baseVertex = basevertex[i];

        multidraw.indexOffset /= IdxSize;

        multidraw.name =
            StringFormat::Fmt("%s[%i](%u)", ToStr(gl_CurChunk).c_str(), i, multidraw.numIndices);

        multidraw.flags |= DrawFlags::Drawcall | DrawFlags::UseIBuffer;

        multidraw.topology = MakePrimitiveTopology(m_Real, mode);
        multidraw.indexByteWidth = IdxSize;

        AddEvent();
        AddDrawcall(multidraw, true);
      }

      m_DrawcallStack.pop_back();
    }
    else if(IsActiveReplaying(m_State))
    {
      size_t i = 0;
      for(; i < m_Events.size(); i++)
      {
        if(m_Events[i].eventId >= m_CurEventID)
          break;
      }

      while(i > 1 && m_Events[i - 1].fileOffset == m_Events[i].fileOffset)
        i--;

      uint32_t baseEventID = m_Events[i].eventId;

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
        m_Real.glMultiDrawElementsBaseVertex(
            mode, count, type, inds.data(),
            RDCMIN((uint32_t)drawcount, m_LastEventID - baseEventID + 1), basevertex);
      }
      else
      {
        // otherwise we do the 'hard' case, draw only one multidraw
        // note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
        // a single draw.
        RDCASSERT(m_LastEventID == m_FirstEventID);

        uint32_t drawidx = (m_LastEventID - baseEventID);

        m_Real.glDrawElementsBaseVertex(mode, count[drawidx], type, inds[drawidx],
                                        basevertex[drawidx]);
      }

      m_CurEventID += (uint32_t)drawcount;
    }
  }

  return true;
}

void WrappedOpenGL::glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei *count, GLenum type,
                                                  const void *const *indices, GLsizei drawcount,
                                                  const GLint *basevertex)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(
      m_Real.glMultiDrawElementsBaseVertex(mode, count, type, indices, drawcount, basevertex));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glMultiDrawElementsBaseVertex(ser, mode, count, type, indices, drawcount, basevertex);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glMultiDrawArraysIndirect(SerialiserType &ser, GLenum mode,
                                                        const void *indirect, GLsizei drawcount,
                                                        GLsizei stride)
{
  SERIALISE_ELEMENT(mode);
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)indirect);
  SERIALISE_ELEMENT(drawcount);
  SERIALISE_ELEMENT(stride);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
    {
      m_Real.glMultiDrawArraysIndirect(mode, (const void *)offset, drawcount, stride);

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(%i)", ToStr(gl_CurChunk).c_str(), drawcount);

      draw.flags |= DrawFlags::MultiDraw;

      draw.topology = MakePrimitiveTopology(m_Real, mode);

      AddDrawcall(draw, false);

      m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());

      {
        GLuint buf = 0;
        m_Real.glGetIntegerv(eGL_DRAW_INDIRECT_BUFFER_BINDING, (GLint *)&buf);

        m_ResourceUses[GetResourceManager()->GetID(BufferRes(GetCtx(), buf))].push_back(
            EventUsage(m_CurEventID, ResourceUsage::Indirect));
      }

      GLintptr offs = (GLintptr)offset;

      SDChunk *baseChunk = m_StructuredFile->chunks.back();

      for(GLsizei i = 0; i < drawcount; i++)
      {
        m_CurEventID++;

        DrawArraysIndirectCommand params;

        m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);

        if(stride)
          offs += stride;
        else
          offs += sizeof(params);

        DrawcallDescription multidraw;
        multidraw.drawIndex = i;
        multidraw.numIndices = params.count;
        multidraw.numInstances = params.instanceCount;
        multidraw.vertexOffset = params.first;
        multidraw.instanceOffset = params.baseInstance;

        multidraw.name = StringFormat::Fmt("%s[%i](<%u, %u>)", ToStr(gl_CurChunk).c_str(), i,
                                           multidraw.numIndices, multidraw.numInstances);

        multidraw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::Indirect;

        multidraw.topology = MakePrimitiveTopology(m_Real, mode);

        // add a fake chunk for this individual indirect draw
        SDChunk *fakeChunk = new SDChunk(multidraw.name.c_str());
        fakeChunk->metadata.chunkID = (uint32_t)GLChunk::glIndirectSubCommand;
        // just copy the metadata
        fakeChunk->metadata = baseChunk->metadata;

        fakeChunk->AddChild(makeSDObject("drawIndex", (uint32_t)i));
        fakeChunk->AddChild(makeSDObject("offset", (uint64_t)offs));

        SDObject *command = new SDObject("command", "DrawArraysIndirectCommand");

        command->type.basetype = SDBasic::Struct;
        command->type.byteSize = sizeof(DrawArraysIndirectCommand);

        command->AddChild(makeSDObject("count", params.count));
        command->AddChild(makeSDObject("instanceCount", params.instanceCount));
        command->AddChild(makeSDObject("first", params.first));
        command->AddChild(makeSDObject("baseInstance", params.baseInstance));

        fakeChunk->AddChild(command);

        m_StructuredFile->chunks.push_back(fakeChunk);

        AddEvent();
        AddDrawcall(multidraw, true);
      }

      m_DrawcallStack.pop_back();
    }
    else if(IsActiveReplaying(m_State))
    {
      size_t i = 0;
      for(; i < m_Events.size(); i++)
      {
        if(m_Events[i].eventId >= m_CurEventID)
          break;
      }

      while(i > 1 && m_Events[i - 1].fileOffset == m_Events[i].fileOffset)
        i--;

      uint32_t baseEventID = m_Events[i].eventId;

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
            mode, (const void *)offset,
            RDCMIN((uint32_t)drawcount, m_LastEventID - baseEventID + 1), stride);
      }
      else
      {
        // otherwise we do the 'hard' case, draw only one multidraw
        // note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
        // a single draw.
        RDCASSERT(m_LastEventID == m_FirstEventID);

        uint32_t drawidx = (m_LastEventID - baseEventID);

        DrawArraysIndirectCommand params;

        GLintptr offs = (GLintptr)offset;
        if(stride != 0)
          offs += stride * drawidx;
        else
          offs += sizeof(params) * drawidx;

        m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);

        m_Real.glDrawArraysInstancedBaseInstance(mode, params.first, params.count,
                                                 params.instanceCount, params.baseInstance);
      }

      m_CurEventID += drawcount;
    }
  }

  return true;
}

void WrappedOpenGL::glMultiDrawArraysIndirect(GLenum mode, const void *indirect, GLsizei drawcount,
                                              GLsizei stride)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glMultiDrawArraysIndirect(mode, indirect, drawcount, stride));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glMultiDrawArraysIndirect(ser, mode, indirect, drawcount, stride);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glMultiDrawElementsIndirect(SerialiserType &ser, GLenum mode,
                                                          GLenum type, const void *indirect,
                                                          GLsizei drawcount, GLsizei stride)
{
  SERIALISE_ELEMENT(mode);
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)indirect);
  SERIALISE_ELEMENT(drawcount);
  SERIALISE_ELEMENT(stride);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  uint32_t IdxSize = GetIdxSize(type);

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
    {
      GLRenderState state(&m_Real);
      state.FetchState(this);

      m_Real.glMultiDrawElementsIndirect(mode, type, (const void *)offset, drawcount, stride);

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(%i)", ToStr(gl_CurChunk).c_str(), drawcount);

      draw.flags |= DrawFlags::MultiDraw;

      draw.topology = MakePrimitiveTopology(m_Real, mode);
      draw.indexByteWidth = IdxSize;

      AddDrawcall(draw, false);

      m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());

      {
        GLuint buf = 0;
        m_Real.glGetIntegerv(eGL_DRAW_INDIRECT_BUFFER_BINDING, (GLint *)&buf);

        m_ResourceUses[GetResourceManager()->GetID(BufferRes(GetCtx(), buf))].push_back(
            EventUsage(m_CurEventID, ResourceUsage::Indirect));
      }

      GLintptr offs = (GLintptr)offset;

      SDChunk *baseChunk = m_StructuredFile->chunks.back();

      for(GLsizei i = 0; i < drawcount; i++)
      {
        m_CurEventID++;

        DrawElementsIndirectCommand params;

        m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);

        if(stride)
          offs += stride;
        else
          offs += sizeof(params);

        DrawcallDescription multidraw;
        multidraw.drawIndex = i;
        multidraw.numIndices = params.count;
        multidraw.numInstances = params.instanceCount;
        multidraw.indexOffset = params.firstIndex;
        multidraw.baseVertex = params.baseVertex;
        multidraw.instanceOffset = params.baseInstance;

        multidraw.name = StringFormat::Fmt("%s[%i](<%u, %u>)", ToStr(gl_CurChunk).c_str(), i,
                                           multidraw.numIndices, multidraw.numInstances);

        multidraw.flags |= DrawFlags::Drawcall | DrawFlags::UseIBuffer | DrawFlags::Instanced |
                           DrawFlags::Indirect;

        multidraw.topology = MakePrimitiveTopology(m_Real, mode);
        multidraw.indexByteWidth = IdxSize;

        // add a fake chunk for this individual indirect draw
        SDChunk *fakeChunk = new SDChunk(multidraw.name.c_str());
        fakeChunk->metadata.chunkID = (uint32_t)GLChunk::glIndirectSubCommand;
        // just copy the metadata
        fakeChunk->metadata = baseChunk->metadata;

        fakeChunk->AddChild(makeSDObject("drawIndex", (uint32_t)i));
        fakeChunk->AddChild(makeSDObject("offset", (uint64_t)offs));

        SDObject *command = new SDObject("command", "DrawElementsIndirectCommand");

        command->type.basetype = SDBasic::Struct;
        command->type.byteSize = sizeof(DrawElementsIndirectCommand);

        command->AddChild(makeSDObject("count", params.count));
        command->AddChild(makeSDObject("instanceCount", params.instanceCount));
        command->AddChild(makeSDObject("firstIndex", params.firstIndex));
        command->AddChild(makeSDObject("baseVertex", params.baseVertex));
        command->AddChild(makeSDObject("baseInstance", params.baseInstance));

        fakeChunk->AddChild(command);

        m_StructuredFile->chunks.push_back(fakeChunk);

        AddEvent();
        AddDrawcall(multidraw, true);
      }

      m_DrawcallStack.pop_back();
    }
    else if(IsActiveReplaying(m_State))
    {
      size_t i = 0;
      for(; i < m_Events.size(); i++)
      {
        if(m_Events[i].eventId >= m_CurEventID)
          break;
      }

      while(i > 1 && m_Events[i - 1].fileOffset == m_Events[i].fileOffset)
        i--;

      uint32_t baseEventID = m_Events[i].eventId;

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
        m_Real.glMultiDrawElementsIndirect(
            mode, type, (const void *)offset,
            RDCMIN((uint32_t)drawcount, m_LastEventID - baseEventID + 1), stride);
      }
      else
      {
        // otherwise we do the 'hard' case, draw only one multidraw
        // note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
        // a single draw.
        RDCASSERT(m_LastEventID == m_FirstEventID);

        uint32_t drawidx = (m_LastEventID - baseEventID);

        DrawElementsIndirectCommand params;

        GLintptr offs = (GLintptr)offset;
        if(stride != 0)
          offs += stride * drawidx;
        else
          offs += sizeof(params) * drawidx;

        m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);

        m_Real.glDrawElementsInstancedBaseVertexBaseInstance(
            mode, params.count, type, (const void *)(ptrdiff_t(params.firstIndex) * IdxSize),
            params.instanceCount, params.baseVertex, params.baseInstance);
      }

      m_CurEventID += drawcount;
    }
  }

  return true;
}

void WrappedOpenGL::glMultiDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect,
                                                GLsizei drawcount, GLsizei stride)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glMultiDrawElementsIndirect(mode, type, indirect, drawcount, stride));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glMultiDrawElementsIndirect(ser, mode, type, indirect, drawcount, stride);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glMultiDrawArraysIndirectCountARB(SerialiserType &ser, GLenum mode,
                                                                const void *indirect,
                                                                GLintptr drawcountPtr,
                                                                GLsizei maxdrawcount, GLsizei stride)
{
  SERIALISE_ELEMENT(mode);
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)indirect);
  SERIALISE_ELEMENT_LOCAL(drawcount, (uint64_t)drawcountPtr);
  SERIALISE_ELEMENT(maxdrawcount);
  SERIALISE_ELEMENT(stride);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLsizei realdrawcount = 0;

    m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, (GLintptr)drawcount, sizeof(realdrawcount),
                              &realdrawcount);

    realdrawcount = RDCMIN(maxdrawcount, realdrawcount);

    if(IsLoading(m_State))
    {
      m_Real.glMultiDrawArraysIndirectCountARB(mode, (const void *)offset, (GLintptr)drawcount,
                                               maxdrawcount, stride);

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(<%i>)", ToStr(gl_CurChunk).c_str(), realdrawcount);

      draw.flags |= DrawFlags::MultiDraw;

      draw.topology = MakePrimitiveTopology(m_Real, mode);

      AddDrawcall(draw, false);

      m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());

      {
        GLuint buf = 0;
        m_Real.glGetIntegerv(eGL_DRAW_INDIRECT_BUFFER_BINDING, (GLint *)&buf);

        m_ResourceUses[GetResourceManager()->GetID(BufferRes(GetCtx(), buf))].push_back(
            EventUsage(m_CurEventID, ResourceUsage::Indirect));
      }

      GLintptr offs = (GLintptr)offset;

      SDChunk *baseChunk = m_StructuredFile->chunks.back();

      for(GLsizei i = 0; i < realdrawcount; i++)
      {
        m_CurEventID++;

        DrawArraysIndirectCommand params;

        m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);

        if(stride)
          offs += stride;
        else
          offs += sizeof(params);

        DrawcallDescription multidraw;
        multidraw.drawIndex = i;
        multidraw.numIndices = params.count;
        multidraw.numInstances = params.instanceCount;
        multidraw.vertexOffset = params.first;
        multidraw.instanceOffset = params.baseInstance;

        multidraw.name = StringFormat::Fmt("%s[%i](<%u, %u>)", ToStr(gl_CurChunk).c_str(), i,
                                           multidraw.numIndices, multidraw.numInstances);

        multidraw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::Indirect;

        multidraw.topology = MakePrimitiveTopology(m_Real, mode);

        // add a fake chunk for this individual indirect draw
        SDChunk *fakeChunk = new SDChunk(multidraw.name.c_str());
        fakeChunk->metadata.chunkID = (uint32_t)GLChunk::glIndirectSubCommand;
        // just copy the metadata
        fakeChunk->metadata = baseChunk->metadata;

        fakeChunk->AddChild(makeSDObject("drawIndex", (uint32_t)i));
        fakeChunk->AddChild(makeSDObject("offset", (uint64_t)offs));

        SDObject *command = new SDObject("command", "DrawArraysIndirectCommand");

        command->type.basetype = SDBasic::Struct;
        command->type.byteSize = sizeof(DrawArraysIndirectCommand);

        command->AddChild(makeSDObject("count", params.count));
        command->AddChild(makeSDObject("instanceCount", params.instanceCount));
        command->AddChild(makeSDObject("first", params.first));
        command->AddChild(makeSDObject("baseInstance", params.baseInstance));

        fakeChunk->AddChild(command);

        m_StructuredFile->chunks.push_back(fakeChunk);

        AddEvent();
        AddDrawcall(multidraw, true);
      }

      m_DrawcallStack.pop_back();
    }
    else if(IsActiveReplaying(m_State))
    {
      size_t i = 0;
      for(; i < m_Events.size(); i++)
      {
        if(m_Events[i].eventId >= m_CurEventID)
          break;
      }

      while(i > 1 && m_Events[i - 1].fileOffset == m_Events[i].fileOffset)
        i--;

      uint32_t baseEventID = m_Events[i].eventId;

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
            mode, (const void *)offset,
            RDCMIN((uint32_t)drawcount, m_LastEventID - baseEventID + 1), stride);
      }
      else
      {
        // otherwise we do the 'hard' case, draw only one multidraw
        // note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
        // a single draw.
        RDCASSERT(m_LastEventID == m_FirstEventID);

        uint32_t drawidx = (m_LastEventID - baseEventID);

        DrawArraysIndirectCommand params;

        GLintptr offs = (GLintptr)offset;
        if(stride != 0)
          offs += stride * drawidx;
        else
          offs += sizeof(params) * drawidx;

        m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);

        m_Real.glDrawArraysInstancedBaseInstance(mode, params.first, params.count,
                                                 params.instanceCount, params.baseInstance);
      }

      m_CurEventID += realdrawcount;
    }
  }

  return true;
}

void WrappedOpenGL::glMultiDrawArraysIndirectCountARB(GLenum mode, const void *indirect,
                                                      GLintptr drawcount, GLsizei maxdrawcount,
                                                      GLsizei stride)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(
      m_Real.glMultiDrawArraysIndirectCountARB(mode, indirect, drawcount, maxdrawcount, stride));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glMultiDrawArraysIndirectCountARB(ser, mode, indirect, drawcount, maxdrawcount, stride);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glMultiDrawElementsIndirectCountARB(SerialiserType &ser, GLenum mode,
                                                                  GLenum type, const void *indirect,
                                                                  GLintptr drawcountPtr,
                                                                  GLsizei maxdrawcount,
                                                                  GLsizei stride)
{
  SERIALISE_ELEMENT(mode);
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)indirect);
  SERIALISE_ELEMENT_LOCAL(drawcount, (uint64_t)drawcountPtr);
  SERIALISE_ELEMENT(maxdrawcount);
  SERIALISE_ELEMENT(stride);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    uint32_t IdxSize = GetIdxSize(type);

    GLsizei realdrawcount = 0;

    m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, (GLintptr)drawcount, sizeof(realdrawcount),
                              &realdrawcount);

    realdrawcount = RDCMIN(maxdrawcount, realdrawcount);

    if(IsLoading(m_State))
    {
      m_Real.glMultiDrawElementsIndirectCountARB(mode, type, (const void *)offset,
                                                 (GLintptr)drawcount, maxdrawcount, stride);

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(<%i>)", ToStr(gl_CurChunk).c_str(), realdrawcount);

      draw.flags |= DrawFlags::MultiDraw;

      draw.topology = MakePrimitiveTopology(m_Real, mode);
      draw.indexByteWidth = IdxSize;

      AddDrawcall(draw, false);

      m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());

      {
        GLuint buf = 0;
        m_Real.glGetIntegerv(eGL_DRAW_INDIRECT_BUFFER_BINDING, (GLint *)&buf);

        m_ResourceUses[GetResourceManager()->GetID(BufferRes(GetCtx(), buf))].push_back(
            EventUsage(m_CurEventID, ResourceUsage::Indirect));
      }

      GLintptr offs = (GLintptr)offset;

      SDChunk *baseChunk = m_StructuredFile->chunks.back();

      for(GLsizei i = 0; i < realdrawcount; i++)
      {
        m_CurEventID++;

        DrawElementsIndirectCommand params;

        m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);

        if(stride)
          offs += stride;
        else
          offs += sizeof(params);

        DrawcallDescription multidraw;
        multidraw.drawIndex = i;
        multidraw.numIndices = params.count;
        multidraw.numInstances = params.instanceCount;
        multidraw.indexOffset = params.firstIndex;
        multidraw.baseVertex = params.baseVertex;
        multidraw.instanceOffset = params.baseInstance;

        multidraw.name = StringFormat::Fmt("%s[%i](<%u, %u>)", ToStr(gl_CurChunk).c_str(), i,
                                           multidraw.numIndices, multidraw.numInstances);

        multidraw.flags |= DrawFlags::Drawcall | DrawFlags::UseIBuffer | DrawFlags::Instanced |
                           DrawFlags::Indirect;

        multidraw.topology = MakePrimitiveTopology(m_Real, mode);
        multidraw.indexByteWidth = IdxSize;

        // add a fake chunk for this individual indirect draw
        SDChunk *fakeChunk = new SDChunk(multidraw.name.c_str());
        fakeChunk->metadata.chunkID = (uint32_t)GLChunk::glIndirectSubCommand;
        // just copy the metadata
        fakeChunk->metadata = baseChunk->metadata;

        fakeChunk->AddChild(makeSDObject("drawIndex", (uint32_t)i));
        fakeChunk->AddChild(makeSDObject("offset", (uint64_t)offs));

        SDObject *command = new SDObject("command", "DrawElementsIndirectCommand");

        command->type.basetype = SDBasic::Struct;
        command->type.byteSize = sizeof(DrawElementsIndirectCommand);

        command->AddChild(makeSDObject("count", params.count));
        command->AddChild(makeSDObject("instanceCount", params.instanceCount));
        command->AddChild(makeSDObject("firstIndex", params.firstIndex));
        command->AddChild(makeSDObject("baseVertex", params.baseVertex));
        command->AddChild(makeSDObject("baseInstance", params.baseInstance));

        fakeChunk->AddChild(command);

        m_StructuredFile->chunks.push_back(fakeChunk);

        AddEvent();
        AddDrawcall(multidraw, true);
      }

      m_DrawcallStack.pop_back();
    }
    else if(IsActiveReplaying(m_State))
    {
      size_t i = 0;
      for(; i < m_Events.size(); i++)
      {
        if(m_Events[i].eventId >= m_CurEventID)
          break;
      }

      while(i > 1 && m_Events[i - 1].fileOffset == m_Events[i].fileOffset)
        i--;

      uint32_t baseEventID = m_Events[i].eventId;

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
        m_Real.glMultiDrawElementsIndirect(
            mode, type, (const void *)offset,
            RDCMIN((uint32_t)drawcount, m_LastEventID - baseEventID + 1), stride);
      }
      else
      {
        // otherwise we do the 'hard' case, draw only one multidraw
        // note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
        // a single draw.
        RDCASSERT(m_LastEventID == m_FirstEventID);

        uint32_t drawidx = (m_LastEventID - baseEventID);

        DrawElementsIndirectCommand params;

        GLintptr offs = (GLintptr)offset;
        if(stride != 0)
          offs += stride * drawidx;
        else
          offs += sizeof(params) * drawidx;

        m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);

        m_Real.glDrawElementsInstancedBaseVertexBaseInstance(
            mode, params.count, type, (const void *)(ptrdiff_t(params.firstIndex) * IdxSize),
            params.instanceCount, params.baseVertex, params.baseInstance);
      }

      m_CurEventID += realdrawcount;
    }
  }

  return true;
}

void WrappedOpenGL::glMultiDrawElementsIndirectCountARB(GLenum mode, GLenum type,
                                                        const void *indirect, GLintptr drawcount,
                                                        GLsizei maxdrawcount, GLsizei stride)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glMultiDrawElementsIndirectCountARB(mode, type, indirect, drawcount,
                                                                 maxdrawcount, stride));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glMultiDrawElementsIndirectCountARB(ser, mode, type, indirect, drawcount,
                                                  maxdrawcount, stride);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glClearNamedFramebufferfv(SerialiserType &ser,
                                                        GLuint framebufferHandle, GLenum buffer,
                                                        GLint drawbuffer, const GLfloat *value)
{
  SERIALISE_ELEMENT_LOCAL(framebuffer, FramebufferRes(GetCtx(), framebufferHandle));
  SERIALISE_ELEMENT(buffer);
  SERIALISE_ELEMENT(drawbuffer);
  SERIALISE_ELEMENT_ARRAY(value, buffer == eGL_DEPTH ? 1 : 4);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // use ARB_direct_state_access functions here as we use EXT_direct_state_access elsewhere. If
    // we are running without ARB_dsa support, these functions are emulated in the obvious way. This
    // is necessary since these functions can be serialised even if ARB_dsa was not used originally,
    // and we need to support this case.
    m_Real.glClearNamedFramebufferfv(framebuffer.name, buffer, drawbuffer, value);

    if(IsLoading(m_State))
    {
      AddEvent();

      std::string name;

      if(buffer == eGL_DEPTH)
        name = StringFormat::Fmt("%s(%s, %i, %f)", ToStr(gl_CurChunk).c_str(),
                                 ToStr(buffer).c_str(), drawbuffer, value[0]);
      else
        name = StringFormat::Fmt("%s(%s, %i, %f, %f, %f, %f)", ToStr(gl_CurChunk).c_str(),
                                 ToStr(buffer).c_str(), drawbuffer, value[0], value[1], value[2],
                                 value[2]);

      DrawcallDescription draw;
      draw.name = name;
      draw.flags |= DrawFlags::Clear;
      if(buffer == eGL_COLOR)
        draw.flags |= DrawFlags::ClearColor;
      else
        draw.flags |= DrawFlags::ClearDepthStencil;

      GLuint attachment = 0;
      GLenum attachName =
          buffer == eGL_COLOR ? GLenum(eGL_COLOR_ATTACHMENT0 + drawbuffer) : eGL_DEPTH_ATTACHMENT;
      GLenum type = eGL_TEXTURE;
      m_Real.glGetNamedFramebufferAttachmentParameterivEXT(
          framebuffer.name, attachName, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&attachment);
      m_Real.glGetNamedFramebufferAttachmentParameterivEXT(
          framebuffer.name, attachName, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

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
  }

  return true;
}

void WrappedOpenGL::glClearNamedFramebufferfv(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                              const GLfloat *value)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glClearNamedFramebufferfv(framebuffer, buffer, drawbuffer, value));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glClearNamedFramebufferfv(ser, framebuffer, buffer, drawbuffer, value);

    GetContextRecord()->AddChunk(scope.Get());

    GLRenderState state(&m_Real);
    state.FetchState(this);
    state.MarkReferenced(this, false);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLRenderState state(&m_Real);
    state.MarkDirty(this);
  }
}

void WrappedOpenGL::glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glClearBufferfv(buffer, drawbuffer, value));

  if(IsActiveCapturing(m_State))
  {
    GLuint framebuffer = 0;
    if(GetCtxData().m_DrawFramebufferRecord)
      framebuffer = GetCtxData().m_DrawFramebufferRecord->Resource.name;

    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glClearNamedFramebufferfv(ser, framebuffer, buffer, drawbuffer, value);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glClearNamedFramebufferiv(SerialiserType &ser,
                                                        GLuint framebufferHandle, GLenum buffer,
                                                        GLint drawbuffer, const GLint *value)
{
  SERIALISE_ELEMENT_LOCAL(framebuffer, FramebufferRes(GetCtx(), framebufferHandle));
  SERIALISE_ELEMENT(buffer);
  SERIALISE_ELEMENT(drawbuffer);
  SERIALISE_ELEMENT_ARRAY(value, buffer == eGL_STENCIL ? 1 : 4);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // use ARB_direct_state_access functions here as we use EXT_direct_state_access elsewhere. If
    // we are running without ARB_dsa support, these functions are emulated in the obvious way. This
    // is necessary since these functions can be serialised even if ARB_dsa was not used originally,
    // and we need to support this case.
    m_Real.glClearNamedFramebufferiv(framebuffer.name, buffer, drawbuffer, value);

    if(IsLoading(m_State))
    {
      AddEvent();

      std::string name;

      if(buffer == eGL_STENCIL)
        name = StringFormat::Fmt("%s(%s, %i, %i)", ToStr(gl_CurChunk).c_str(),
                                 ToStr(buffer).c_str(), drawbuffer, value[0]);
      else
        name = StringFormat::Fmt("%s(%s, %i, %i, %i, %i, %i)", ToStr(gl_CurChunk).c_str(),
                                 ToStr(buffer).c_str(), drawbuffer, value[0], value[1], value[2],
                                 value[3]);

      DrawcallDescription draw;
      draw.name = name;
      draw.flags |= DrawFlags::Clear;
      if(buffer == eGL_COLOR)
        draw.flags |= DrawFlags::ClearColor;
      else
        draw.flags |= DrawFlags::ClearDepthStencil;

      GLuint attachment = 0;
      GLenum attachName =
          buffer == eGL_COLOR ? GLenum(eGL_COLOR_ATTACHMENT0 + drawbuffer) : eGL_STENCIL_ATTACHMENT;
      GLenum type = eGL_TEXTURE;
      m_Real.glGetNamedFramebufferAttachmentParameterivEXT(
          framebuffer.name, attachName, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&attachment);
      m_Real.glGetNamedFramebufferAttachmentParameterivEXT(
          framebuffer.name, attachName, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

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
  }

  return true;
}

void WrappedOpenGL::glClearNamedFramebufferiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                              const GLint *value)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glClearNamedFramebufferiv(framebuffer, buffer, drawbuffer, value));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glClearNamedFramebufferiv(ser, framebuffer, buffer, drawbuffer, value);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *value)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glClearBufferiv(buffer, drawbuffer, value));

  if(IsActiveCapturing(m_State))
  {
    GLuint framebuffer = 0;
    if(GetCtxData().m_DrawFramebufferRecord)
      framebuffer = GetCtxData().m_DrawFramebufferRecord->Resource.name;

    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glClearNamedFramebufferiv(ser, framebuffer, buffer, drawbuffer, value);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glClearNamedFramebufferuiv(SerialiserType &ser,
                                                         GLuint framebufferHandle, GLenum buffer,
                                                         GLint drawbuffer, const GLuint *value)
{
  SERIALISE_ELEMENT_LOCAL(framebuffer, FramebufferRes(GetCtx(), framebufferHandle));
  SERIALISE_ELEMENT(buffer);
  SERIALISE_ELEMENT(drawbuffer);
  SERIALISE_ELEMENT_ARRAY(value, 4);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // use ARB_direct_state_access functions here as we use EXT_direct_state_access elsewhere. If
    // we are running without ARB_dsa support, these functions are emulated in the obvious way. This
    // is necessary since these functions can be serialised even if ARB_dsa was not used originally,
    // and we need to support this case.
    m_Real.glClearNamedFramebufferuiv(framebuffer.name, buffer, drawbuffer, value);

    if(IsLoading(m_State))
    {
      AddEvent();

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(%s, %i, %u, %u, %u, %u)", ToStr(gl_CurChunk).c_str(),
                                    ToStr(buffer).c_str(), drawbuffer, value[0], value[1], value[2],
                                    value[3]);

      draw.flags |= DrawFlags::Clear | DrawFlags::ClearColor;

      GLuint attachment = 0;
      GLenum attachName = GLenum(eGL_COLOR_ATTACHMENT0 + drawbuffer);
      GLenum type = eGL_TEXTURE;
      m_Real.glGetNamedFramebufferAttachmentParameterivEXT(
          framebuffer.name, attachName, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&attachment);
      m_Real.glGetNamedFramebufferAttachmentParameterivEXT(
          framebuffer.name, attachName, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

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
  }

  return true;
}

void WrappedOpenGL::glClearNamedFramebufferuiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                               const GLuint *value)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glClearNamedFramebufferuiv(framebuffer, buffer, drawbuffer, value));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glClearNamedFramebufferuiv(ser, framebuffer, buffer, drawbuffer, value);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *value)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glClearBufferuiv(buffer, drawbuffer, value));

  if(IsActiveCapturing(m_State))
  {
    GLuint framebuffer = 0;
    if(GetCtxData().m_DrawFramebufferRecord)
      framebuffer = GetCtxData().m_DrawFramebufferRecord->Resource.name;

    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glClearNamedFramebufferuiv(ser, framebuffer, buffer, drawbuffer, value);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glClearNamedFramebufferfi(SerialiserType &ser, GLuint framebufferHandle,
                                                        GLenum buffer, GLint drawbuffer,
                                                        GLfloat depth, GLint stencil)
{
  SERIALISE_ELEMENT_LOCAL(framebuffer, FramebufferRes(GetCtx(), framebufferHandle));
  SERIALISE_ELEMENT(buffer);
  SERIALISE_ELEMENT(drawbuffer);
  SERIALISE_ELEMENT(depth);
  SERIALISE_ELEMENT(stencil);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // use ARB_direct_state_access functions here as we use EXT_direct_state_access elsewhere. If
    // we are running without ARB_dsa support, these functions are emulated in the obvious way. This
    // is necessary since these functions can be serialised even if ARB_dsa was not used originally,
    // and we need to support this case.
    m_Real.glClearNamedFramebufferfi(framebuffer.name, buffer, drawbuffer, depth, stencil);

    if(IsLoading(m_State))
    {
      AddEvent();

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(%f, %i)", ToStr(gl_CurChunk).c_str(), depth, stencil);
      draw.flags |= DrawFlags::Clear | DrawFlags::ClearDepthStencil;

      GLuint attachment = 0;
      GLenum type = eGL_TEXTURE;
      m_Real.glGetNamedFramebufferAttachmentParameterivEXT(framebuffer.name, eGL_DEPTH_ATTACHMENT,
                                                           eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                           (GLint *)&attachment);
      m_Real.glGetNamedFramebufferAttachmentParameterivEXT(framebuffer.name, eGL_DEPTH_ATTACHMENT,
                                                           eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                           (GLint *)&type);

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
      m_Real.glGetNamedFramebufferAttachmentParameterivEXT(framebuffer.name, eGL_STENCIL_ATTACHMENT,
                                                           eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                           (GLint *)&attachment);
      m_Real.glGetNamedFramebufferAttachmentParameterivEXT(framebuffer.name, eGL_STENCIL_ATTACHMENT,
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
  }

  return true;
}

void WrappedOpenGL::glClearNamedFramebufferfi(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                              GLfloat depth, GLint stencil)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(
      m_Real.glClearNamedFramebufferfi(framebuffer, buffer, drawbuffer, depth, stencil));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glClearNamedFramebufferfi(ser, framebuffer, buffer, drawbuffer, depth, stencil);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glClearBufferfi(buffer, drawbuffer, depth, stencil));

  if(IsActiveCapturing(m_State))
  {
    GLuint framebuffer = 0;
    if(GetCtxData().m_DrawFramebufferRecord)
      framebuffer = GetCtxData().m_DrawFramebufferRecord->Resource.name;

    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glClearNamedFramebufferfi(ser, framebuffer, buffer, drawbuffer, depth, stencil);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glClearNamedBufferDataEXT(SerialiserType &ser, GLuint bufferHandle,
                                                        GLenum internalformat, GLenum format,
                                                        GLenum type, const void *dataPtr)
{
  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle));
  SERIALISE_ELEMENT(internalformat);
  SERIALISE_ELEMENT(format);
  SERIALISE_ELEMENT(type);

  uint64_t data[4] = {0};

  if(ser.IsWriting())
  {
    size_t s = 1;
    switch(format)
    {
      default:
        RDCWARN("Unexpected format %x, defaulting to single component", format);
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
    switch(type)
    {
      case eGL_UNSIGNED_BYTE:
      case eGL_BYTE: s *= 1; break;
      case eGL_UNSIGNED_SHORT:
      case eGL_SHORT: s *= 2; break;
      case eGL_UNSIGNED_INT:
      case eGL_INT:
      case eGL_FLOAT: s *= 4; break;
      default:
        RDCWARN("Unexpected type %x, defaulting to 1 byte type", type);
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
    if(dataPtr)
      memcpy(data, dataPtr, s);
    else
      memset(data, 0, s);
  }

  SERIALISE_ELEMENT(data);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glClearNamedBufferDataEXT(buffer.name, internalformat, format, type,
                                     (const void *)&data[0]);
  }

  return true;
}

void WrappedOpenGL::glClearNamedBufferDataEXT(GLuint buffer, GLenum internalformat, GLenum format,
                                              GLenum type, const void *data)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glClearNamedBufferDataEXT(buffer, internalformat, format, type, data));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glClearNamedBufferDataEXT(ser, buffer, internalformat, format, type, data);

    GetContextRecord()->AddChunk(scope.Get());
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GetResourceManager()->MarkDirtyResource(BufferRes(GetCtx(), buffer));
  }
}

void WrappedOpenGL::glClearBufferData(GLenum target, GLenum internalformat, GLenum format,
                                      GLenum type, const void *data)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glClearBufferData(target, internalformat, format, type, data));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetCtxData().m_BufferRecord[BufferIdx(target)];
    RDCASSERTMSG("Couldn't identify implicit object at binding. Mismatched or bad GLuint?", record,
                 target);

    if(record)
    {
      if(IsActiveCapturing(m_State))
      {
        USE_SCRATCH_SERIALISER();

        ser.SetDrawChunk();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glClearNamedBufferDataEXT(ser, record->Resource.name, internalformat, format,
                                            type, data);

        GetContextRecord()->AddChunk(scope.Get());
      }
      else if(IsBackgroundCapturing(m_State))
      {
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glClearNamedBufferSubDataEXT(SerialiserType &ser, GLuint bufferHandle,
                                                           GLenum internalformat, GLintptr offsetPtr,
                                                           GLsizeiptr sizePtr, GLenum format,
                                                           GLenum type, const void *dataPtr)
{
  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle));
  SERIALISE_ELEMENT(internalformat);
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)offsetPtr);
  SERIALISE_ELEMENT_LOCAL(size, (uint64_t)sizePtr);
  SERIALISE_ELEMENT(format);
  SERIALISE_ELEMENT(type);

  uint64_t data[4] = {0};

  if(ser.IsWriting())
  {
    size_t s = 1;
    switch(format)
    {
      default:
        RDCWARN("Unexpected format %x, defaulting to single component", format);
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
    switch(type)
    {
      case eGL_UNSIGNED_BYTE:
      case eGL_BYTE: s *= 1; break;
      case eGL_UNSIGNED_SHORT:
      case eGL_SHORT: s *= 2; break;
      case eGL_UNSIGNED_INT:
      case eGL_INT:
      case eGL_FLOAT: s *= 4; break;
      default:
        RDCWARN("Unexpected type %x, defaulting to 1 byte type", type);
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
    if(dataPtr)
      memcpy(data, dataPtr, s);
    else
      memset(data, 0, s);
  }

  SERIALISE_ELEMENT(data);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glClearNamedBufferSubDataEXT(buffer.name, internalformat, (GLintptr)offset,
                                        (GLsizeiptr)size, format, type, (const void *)&data[0]);
  }

  return true;
}

void WrappedOpenGL::glClearNamedBufferSubDataEXT(GLuint buffer, GLenum internalformat,
                                                 GLintptr offset, GLsizeiptr size, GLenum format,
                                                 GLenum type, const void *data)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glClearNamedBufferSubDataEXT(buffer, internalformat, offset, size,
                                                          format, type, data));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glClearNamedBufferSubDataEXT(ser, buffer, internalformat, offset, size, format, type,
                                           data);

    GetContextRecord()->AddChunk(scope.Get());
  }
  else if(IsBackgroundCapturing(m_State))
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

  SERIALISE_TIME_CALL(
      m_Real.glClearBufferSubData(target, internalformat, offset, size, format, type, data));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetCtxData().m_BufferRecord[BufferIdx(target)];
    RDCASSERTMSG("Couldn't identify implicit object at binding. Mismatched or bad GLuint?", record,
                 target);

    if(record)
    {
      if(IsActiveCapturing(m_State))
      {
        USE_SCRATCH_SERIALISER();

        ser.SetDrawChunk();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glClearNamedBufferSubDataEXT(ser, record->Resource.name, internalformat, offset,
                                               size, format, type, data);

        GetContextRecord()->AddChunk(scope.Get());
      }
      else if(IsBackgroundCapturing(m_State))
      {
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glClear(SerialiserType &ser, GLbitfield mask)
{
  SERIALISE_ELEMENT(mask);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glClear(mask);

    if(IsLoading(m_State))
    {
      AddEvent();
      std::string name = ToStr(gl_CurChunk) + "(";
      if(mask & GL_COLOR_BUFFER_BIT)
      {
        float col[4] = {0};
        m_Real.glGetFloatv(eGL_COLOR_CLEAR_VALUE, &col[0]);
        name += StringFormat::Fmt("Color = <%f, %f, %f, %f>, ", col[0], col[1], col[2], col[3]);
      }
      if(mask & GL_DEPTH_BUFFER_BIT)
      {
        float depth = 0;
        m_Real.glGetFloatv(eGL_DEPTH_CLEAR_VALUE, &depth);
        name += StringFormat::Fmt("Depth = <%f>, ", depth);
      }
      if(mask & GL_STENCIL_BUFFER_BIT)
      {
        GLint stencil = 0;
        m_Real.glGetIntegerv(eGL_STENCIL_CLEAR_VALUE, &stencil);
        name += StringFormat::Fmt("Stencil = <0x%02x>, ", stencil);
      }

      if(mask & (eGL_DEPTH_BUFFER_BIT | eGL_COLOR_BUFFER_BIT | eGL_STENCIL_BUFFER_BIT))
      {
        name.pop_back();    // ','
        name.pop_back();    // ' '
      }

      name += ")";

      DrawcallDescription draw;
      draw.name = name;
      draw.flags |= DrawFlags::Clear;
      if(mask & GL_COLOR_BUFFER_BIT)
        draw.flags |= DrawFlags::ClearColor;
      if(mask & (eGL_DEPTH_BUFFER_BIT | eGL_STENCIL_BUFFER_BIT))
        draw.flags |= DrawFlags::ClearDepthStencil;

      AddDrawcall(draw, true);

      GLuint attachment = 0;
      GLenum type = eGL_TEXTURE;

      if(mask & GL_DEPTH_BUFFER_BIT)
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

      if(mask & GL_STENCIL_BUFFER_BIT)
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

      if(mask & GL_COLOR_BUFFER_BIT)
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
              m_ResourceUses[GetResourceManager()->GetID(RenderbufferRes(GetCtx(), attachment))]
                  .push_back(EventUsage(m_CurEventID, ResourceUsage::Clear));
          }
        }
      }
    }
  }

  return true;
}

void WrappedOpenGL::glClear(GLbitfield mask)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glClear(mask));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glClear(ser, mask);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glClearTexImage(SerialiserType &ser, GLuint textureHandle, GLint level,
                                              GLenum format, GLenum type, const void *dataPtr)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(format);
  SERIALISE_ELEMENT(type);

  uint64_t data[4] = {0};

  if(ser.IsWriting())
  {
    size_t s = 1;
    switch(format)
    {
      default:
        RDCWARN("Unexpected format %x, defaulting to single component", format);
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
    switch(type)
    {
      case eGL_UNSIGNED_BYTE:
      case eGL_BYTE: s *= 1; break;
      case eGL_UNSIGNED_SHORT:
      case eGL_SHORT: s *= 2; break;
      case eGL_UNSIGNED_INT:
      case eGL_INT:
      case eGL_FLOAT: s *= 4; break;
      default:
        RDCWARN("Unexpected type %x, defaulting to 1 byte type", type);
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
    if(dataPtr)
      memcpy(data, dataPtr, s);
    else
      memset(data, 0, s);
  }

  SERIALISE_ELEMENT(data);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glClearTexImage(texture.name, level, format, type, (const void *)&data[0]);
  }

  return true;
}

void WrappedOpenGL::glClearTexImage(GLuint texture, GLint level, GLenum format, GLenum type,
                                    const void *data)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glClearTexImage(texture, level, format, type, data));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glClearTexImage(ser, texture, level, format, type, data);

    GetContextRecord()->AddChunk(scope.Get());
    m_MissingTracks.insert(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GetResourceManager()->MarkDirtyResource(TextureRes(GetCtx(), texture));
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glClearTexSubImage(SerialiserType &ser, GLuint textureHandle,
                                                 GLint level, GLint xoffset, GLint yoffset,
                                                 GLint zoffset, GLsizei width, GLsizei height,
                                                 GLsizei depth, GLenum format, GLenum type,
                                                 const void *dataPtr)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(xoffset);
  SERIALISE_ELEMENT(yoffset);
  SERIALISE_ELEMENT(zoffset);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);
  SERIALISE_ELEMENT(depth);
  SERIALISE_ELEMENT(format);
  SERIALISE_ELEMENT(type);

  uint64_t data[4] = {0};

  if(ser.IsWriting())
  {
    size_t s = 1;
    switch(format)
    {
      default:
        RDCWARN("Unexpected format %x, defaulting to single component", format);
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
    switch(type)
    {
      case eGL_UNSIGNED_BYTE:
      case eGL_BYTE: s *= 1; break;
      case eGL_UNSIGNED_SHORT:
      case eGL_SHORT: s *= 2; break;
      case eGL_UNSIGNED_INT:
      case eGL_INT:
      case eGL_FLOAT: s *= 4; break;
      default:
        RDCWARN("Unexpected type %x, defaulting to 1 byte type", type);
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
    if(dataPtr)
      memcpy(data, dataPtr, s);
    else
      memset(data, 0, s);
  }

  SERIALISE_ELEMENT(data);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glClearTexSubImage(texture.name, level, xoffset, yoffset, zoffset, width, height, depth,
                              format, type, (const void *)&data[0]);
  }

  return true;
}

void WrappedOpenGL::glClearTexSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                                       GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                       GLenum format, GLenum type, const void *data)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(m_Real.glClearTexSubImage(texture, level, xoffset, yoffset, zoffset, width,
                                                height, depth, format, type, data));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glClearTexSubImage(ser, texture, level, xoffset, yoffset, zoffset, width, height,
                                 depth, format, type, data);

    GetContextRecord()->AddChunk(scope.Get());
    m_MissingTracks.insert(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GetResourceManager()->MarkDirtyResource(TextureRes(GetCtx(), texture));
  }
}

INSTANTIATE_FUNCTION_SERIALISED(void, glDispatchCompute, GLuint num_groups_x, GLuint num_groups_y,
                                GLuint num_groups_z);
INSTANTIATE_FUNCTION_SERIALISED(void, glDispatchComputeGroupSizeARB, GLuint num_groups_x,
                                GLuint num_groups_y, GLuint num_groups_z, GLuint group_size_x,
                                GLuint group_size_y, GLuint group_size_z);
INSTANTIATE_FUNCTION_SERIALISED(void, glDispatchComputeIndirect, GLintptr indirect);
INSTANTIATE_FUNCTION_SERIALISED(void, glMemoryBarrier, GLbitfield barriers);
INSTANTIATE_FUNCTION_SERIALISED(void, glMemoryBarrierByRegion, GLbitfield barriers);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureBarrier);
INSTANTIATE_FUNCTION_SERIALISED(void, glDrawTransformFeedback, GLenum mode, GLuint xfbHandle);
INSTANTIATE_FUNCTION_SERIALISED(void, glDrawTransformFeedbackInstanced, GLenum mode, GLuint id,
                                GLsizei instancecount);
INSTANTIATE_FUNCTION_SERIALISED(void, glDrawTransformFeedbackStream, GLenum mode, GLuint id,
                                GLuint stream);
INSTANTIATE_FUNCTION_SERIALISED(void, glDrawTransformFeedbackStreamInstanced, GLenum mode,
                                GLuint id, GLuint stream, GLsizei instancecount);
INSTANTIATE_FUNCTION_SERIALISED(void, glDrawArrays, GLenum mode, GLint first, GLsizei count);
INSTANTIATE_FUNCTION_SERIALISED(void, glDrawArraysIndirect, GLenum mode, const void *indirect);
INSTANTIATE_FUNCTION_SERIALISED(void, glDrawArraysInstanced, GLenum mode, GLint first,
                                GLsizei count, GLsizei instancecount);
INSTANTIATE_FUNCTION_SERIALISED(void, glDrawArraysInstancedBaseInstance, GLenum mode, GLint first,
                                GLsizei count, GLsizei instancecount, GLuint baseinstance);
INSTANTIATE_FUNCTION_SERIALISED(void, glDrawElements, GLenum mode, GLsizei count, GLenum type,
                                const void *indicesPtr);
INSTANTIATE_FUNCTION_SERIALISED(void, glDrawElementsIndirect, GLenum mode, GLenum type,
                                const void *indirect);
INSTANTIATE_FUNCTION_SERIALISED(void, glDrawRangeElements, GLenum mode, GLuint start, GLuint end,
                                GLsizei count, GLenum type, const void *indicesPtr);
INSTANTIATE_FUNCTION_SERIALISED(void, glDrawRangeElementsBaseVertex, GLenum mode, GLuint start,
                                GLuint end, GLsizei count, GLenum type, const void *indicesPtr,
                                GLint basevertex);
INSTANTIATE_FUNCTION_SERIALISED(void, glDrawElementsBaseVertex, GLenum mode, GLsizei count,
                                GLenum type, const void *indices, GLint basevertex);
INSTANTIATE_FUNCTION_SERIALISED(void, glDrawElementsInstanced, GLenum mode, GLsizei count,
                                GLenum type, const void *indices, GLsizei instancecount);
INSTANTIATE_FUNCTION_SERIALISED(void, glDrawElementsInstancedBaseInstance, GLenum mode,
                                GLsizei count, GLenum type, const void *indicesPtr,
                                GLsizei instancecount, GLuint baseinstance);
INSTANTIATE_FUNCTION_SERIALISED(void, glDrawElementsInstancedBaseVertex, GLenum mode, GLsizei count,
                                GLenum type, const void *indicesPtr, GLsizei instancecount,
                                GLint basevertex);
INSTANTIATE_FUNCTION_SERIALISED(void, glDrawElementsInstancedBaseVertexBaseInstance, GLenum mode,
                                GLsizei count, GLenum type, const void *indices,
                                GLsizei instancecount, GLint basevertex, GLuint baseinstance);
INSTANTIATE_FUNCTION_SERIALISED(void, glMultiDrawArrays, GLenum mode, const GLint *first,
                                const GLsizei *count, GLsizei drawcount);
INSTANTIATE_FUNCTION_SERIALISED(void, glMultiDrawElements, GLenum mode, const GLsizei *count,
                                GLenum type, const void *const *indices, GLsizei drawcount);
INSTANTIATE_FUNCTION_SERIALISED(void, glMultiDrawElementsBaseVertex, GLenum mode,
                                const GLsizei *count, GLenum type, const void *const *indices,
                                GLsizei drawcount, const GLint *basevertex);
INSTANTIATE_FUNCTION_SERIALISED(void, glMultiDrawArraysIndirect, GLenum mode, const void *indirect,
                                GLsizei drawcount, GLsizei stride);
INSTANTIATE_FUNCTION_SERIALISED(void, glMultiDrawElementsIndirect, GLenum mode, GLenum type,
                                const void *indirect, GLsizei drawcount, GLsizei stride);
INSTANTIATE_FUNCTION_SERIALISED(void, glMultiDrawArraysIndirectCountARB, GLenum mode,
                                const void *indirect, GLintptr drawcount, GLsizei maxdrawcount,
                                GLsizei stride);
INSTANTIATE_FUNCTION_SERIALISED(void, glMultiDrawElementsIndirectCountARB, GLenum mode, GLenum type,
                                const void *indirect, GLintptr drawcount, GLsizei maxdrawcount,
                                GLsizei stride);
INSTANTIATE_FUNCTION_SERIALISED(void, glClearNamedFramebufferfv, GLuint framebufferHandle,
                                GLenum buffer, GLint drawbuffer, const GLfloat *valuePtr);
INSTANTIATE_FUNCTION_SERIALISED(void, glClearNamedFramebufferiv, GLuint framebuffer, GLenum buffer,
                                GLint drawbuffer, const GLint *valuePtr);
INSTANTIATE_FUNCTION_SERIALISED(void, glClearNamedFramebufferuiv, GLuint framebuffer, GLenum buffer,
                                GLint drawbuffer, const GLuint *value);
INSTANTIATE_FUNCTION_SERIALISED(void, glClearNamedFramebufferfi, GLuint framebuffer, GLenum buffer,
                                GLint drawbuffer, GLfloat depth, GLint stencil);
INSTANTIATE_FUNCTION_SERIALISED(void, glClearNamedBufferDataEXT, GLuint bufferHandle,
                                GLenum internalformat, GLenum format, GLenum type,
                                const void *dataPtr);
INSTANTIATE_FUNCTION_SERIALISED(void, glClearNamedBufferSubDataEXT, GLuint buffer,
                                GLenum internalformat, GLintptr offsetPtr, GLsizeiptr sizePtr,
                                GLenum format, GLenum type, const void *dataPtr);
INSTANTIATE_FUNCTION_SERIALISED(void, glClear, GLbitfield mask);
INSTANTIATE_FUNCTION_SERIALISED(void, glClearTexImage, GLuint texture, GLint level, GLenum format,
                                GLenum type, const void *dataPtr);
INSTANTIATE_FUNCTION_SERIALISED(void, glClearTexSubImage, GLuint texture, GLint level, GLint xoffset,
                                GLint yoffset, GLint zoffset, GLsizei width, GLsizei height,
                                GLsizei depth, GLenum format, GLenum type, const void *dataPtr);
