/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 Baldur Karlsson
 * Copyright (c) 2014 Crytek
 * Copyright (c) 2016 University of Szeged
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
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

#include "../gles_driver.h"
#include "common/common.h"
#include "serialise/string_utils.h"

bool WrappedGLES::Serialise_glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y,
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

    FetchDrawcall draw;
    draw.name = name;
    draw.flags |= eDraw_Dispatch;

    draw.dispatchDimension[0] = X;
    draw.dispatchDimension[1] = Y;
    draw.dispatchDimension[2] = Z;

    if(X == 0)
      AddDebugMessage(eDbgCategory_Execution, eDbgSeverity_Medium, eDbgSource_IncorrectAPIUse,
                      "Dispatch call has Num Groups X=0. This will do nothing, which is unusual "
                      "for a non-indirect Dispatch. Did you mean X=1?");
    if(Y == 0)
      AddDebugMessage(eDbgCategory_Execution, eDbgSeverity_Medium, eDbgSource_IncorrectAPIUse,
                      "Dispatch call has Num Groups Y=0. This will do nothing, which is unusual "
                      "for a non-indirect Dispatch. Did you mean Y=1?");
    if(Z == 0)
      AddDebugMessage(eDbgCategory_Execution, eDbgSeverity_Medium, eDbgSource_IncorrectAPIUse,
                      "Dispatch call has Num Groups Z=0. This will do nothing, which is unusual "
                      "for a non-indirect Dispatch. Did you mean Z=1?");

    AddDrawcall(draw, true);
  }

  return true;
}

void WrappedGLES::glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z)
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

bool WrappedGLES::Serialise_glDispatchComputeIndirect(GLintptr indirect)
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
    Compat_glGetBufferSubData(eGL_DISPATCH_INDIRECT_BUFFER, (GLintptr)offs, sizeof(uint32_t) * 3, groupSizes);

    AddEvent(desc);
    string name = "glDispatchComputeIndirect(<" + ToStr::Get(groupSizes[0]) + ", " +
                  ToStr::Get(groupSizes[1]) + ", " + ToStr::Get(groupSizes[2]) + ">)";

    FetchDrawcall draw;
    draw.name = name;
    draw.flags |= eDraw_Dispatch | eDraw_Indirect;

    draw.dispatchDimension[0] = groupSizes[0];
    draw.dispatchDimension[1] = groupSizes[1];
    draw.dispatchDimension[2] = groupSizes[2];

    AddDrawcall(draw, true);

    MarkIndirectBufferUsage();
  }

  return true;
}

void WrappedGLES::glDispatchComputeIndirect(GLintptr indirect)
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

bool WrappedGLES::Serialise_glMemoryBarrier(GLbitfield barriers)
{
  SERIALISE_ELEMENT(uint32_t, Barriers, barriers);

  if(m_State <= EXECUTING)
  {
    m_Real.glMemoryBarrier(Barriers);
  }

  return true;
}

void WrappedGLES::glMemoryBarrier(GLbitfield barriers)
{
  if(barriers & eGL_CLIENT_MAPPED_BUFFER_BARRIER_BIT_EXT)
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

bool WrappedGLES::Serialise_glMemoryBarrierByRegion(GLbitfield barriers)
{
  SERIALISE_ELEMENT(uint32_t, Barriers, barriers);

  if(m_State <= EXECUTING)
  {
    m_Real.glMemoryBarrierByRegion(Barriers);
  }

  return true;
}

void WrappedGLES::glMemoryBarrierByRegion(GLbitfield barriers)
{
  if(barriers & eGL_CLIENT_MAPPED_BUFFER_BARRIER_BIT_EXT)
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

size_t calculateVertexPointerSize(GLint size, GLenum type, GLsizei stride, GLsizei count)
{
    if (count == 0)
      return 0;

    if (size > 4) {
      RDCERR("Unexpected size greater than 4!");
    }

    GLsizei elementSize = size;
    switch (type)
    {
      case eGL_UNSIGNED_BYTE:
      case eGL_BYTE:
        // elementSize = size;
        break;
      case eGL_UNSIGNED_SHORT:
      case eGL_SHORT:
      case eGL_HALF_FLOAT:
        elementSize *= 2;
        break;
      case eGL_UNSIGNED_INT:
      case eGL_INT:
      case eGL_FLOAT:
      case eGL_FIXED:
        elementSize *= 4; break;
      default:
        RDCERR("Unexpected type %x", type);
        break;
      case eGL_UNSIGNED_INT_10_10_10_2_OES:
      case eGL_UNSIGNED_INT_2_10_10_10_REV:
        elementSize *= 4;
        break;
    }

    if (stride == 0)
      stride = elementSize;

    return stride * (count - 1) + elementSize;
}

void WrappedGLES::writeFakeVertexAttribPointer(GLsizei count)
{
    ContextData &cd = GetCtxData();
    GLResourceRecord *bufrecord = cd.GetActiveBufferRecord(eGL_ARRAY_BUFFER);
    GLResourceRecord *varecord = cd.m_VertexArrayRecord;

    // void APIENTRY glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer)
    GLint maxVertexAttrib = 0;
    m_Real.glGetIntegerv(eGL_MAX_VERTEX_ATTRIBS, &maxVertexAttrib);
    for (GLint index = 0; index < maxVertexAttrib; ++index)
    {
        GLint enabled = 0;
        m_Real.glGetVertexAttribiv(index, eGL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
        if (enabled) {
            GLint binding = 0;
            m_Real.glGetVertexAttribiv(index, eGL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &binding);
            if (binding == 0) {
              GLint size = 0;
              m_Real.glGetVertexAttribiv(index, eGL_VERTEX_ATTRIB_ARRAY_SIZE, &size);
              GLint type = 0;
              m_Real.glGetVertexAttribiv(index, eGL_VERTEX_ATTRIB_ARRAY_TYPE, &type);
              GLint normalized = 0;
              m_Real.glGetVertexAttribiv(index, eGL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &normalized);
              GLint stride = 0;
              m_Real.glGetVertexAttribiv(index, eGL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride);
              GLint isInteger = 0;
              m_Real.glGetVertexAttribiv(index, eGL_VERTEX_ATTRIB_ARRAY_INTEGER, &isInteger);
              GLvoid * pointer = 0;
              m_Real.glGetVertexAttribPointerv(index, eGL_VERTEX_ATTRIB_ARRAY_POINTER, &pointer);
              size_t attribDataSize = calculateVertexPointerSize(size, GLenum(type), stride, count);

              SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBPOINTER);
              Serialise_glVertexAttribPointerEXT(
                varecord ? varecord->Resource.name : 0,
                bufrecord ? bufrecord->Resource.name : 0,
                index,
                size,
                GLenum(type),
                normalized,
                stride,
                pointer,
                attribDataSize,
                isInteger != 0);

              m_ContextRecord->AddChunk(scope.Get());
            }
        }
    }
}

bool WrappedGLES::Serialise_glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
  SERIALISE_ELEMENT(GLenum, Mode, mode);
  SERIALISE_ELEMENT(int32_t, First, first);
  SERIALISE_ELEMENT(uint32_t, Count, count);

  if(m_State <= EXECUTING)
  {
    m_Real.glDrawArrays(Mode, First, Count);
    clearLocalDataBuffers();
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);
    string name = "glDrawArrays(" + ToStr::Get(Count) + ")";

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

void WrappedGLES::glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
  CoherentMapImplicitBarrier();

  m_Real.glDrawArrays(mode, first, count);

  if(m_State == WRITING_CAPFRAME)
  {
    writeFakeVertexAttribPointer(count);

    SCOPED_SERIALISE_CONTEXT(DRAWARRAYS);
    Serialise_glDrawArrays(mode, first, count);

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

bool WrappedGLES::Serialise_glDrawArraysIndirect(GLenum mode, const void *indirect)
{
  SERIALISE_ELEMENT(GLenum, Mode, mode);
  SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)indirect);

  if(m_State <= EXECUTING)
  {
    m_Real.glDrawArraysIndirect(Mode, (const void *)Offset);
    clearLocalDataBuffers();
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    DrawArraysIndirectCommand params;
    Compat_glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, (GLintptr)Offset, sizeof(params), &params);

    AddEvent(desc);
    string name = "glDrawArraysIndirect(" + ToStr::Get(params.count) + ", " +
                  ToStr::Get(params.instanceCount) + ">)";

    FetchDrawcall draw;
    draw.name = name;
    draw.numIndices = params.count;
    draw.numInstances = params.instanceCount;
    draw.vertexOffset = params.first;
    draw.instanceOffset = params.baseInstance;

    draw.flags |= eDraw_Drawcall | eDraw_Instanced | eDraw_Indirect;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);

    AddDrawcall(draw, true);

    MarkIndirectBufferUsage();
  }

  return true;
}

void WrappedGLES::glDrawArraysIndirect(GLenum mode, const void *indirect)
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

bool WrappedGLES::Serialise_glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count,
                                                    GLsizei instancecount)
{
  SERIALISE_ELEMENT(GLenum, Mode, mode);
  SERIALISE_ELEMENT(int32_t, First, first);
  SERIALISE_ELEMENT(uint32_t, Count, count);
  SERIALISE_ELEMENT(uint32_t, InstanceCount, instancecount);

  if(m_State <= EXECUTING)
  {
    m_Real.glDrawArraysInstanced(Mode, First, Count, InstanceCount);
    clearLocalDataBuffers();
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);
    string name =
        "glDrawArraysInstanced(" + ToStr::Get(Count) + ", " + ToStr::Get(InstanceCount) + ")";

    FetchDrawcall draw;
    draw.name = name;
    draw.numIndices = Count;
    draw.numInstances = InstanceCount;
    draw.indexOffset = 0;
    draw.vertexOffset = First;
    draw.instanceOffset = 0;

    draw.flags |= eDraw_Drawcall | eDraw_Instanced;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);

    AddDrawcall(draw, true);
  }

  return true;
}

void WrappedGLES::glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count,
                                          GLsizei instancecount)
{
  CoherentMapImplicitBarrier();

  m_Real.glDrawArraysInstanced(mode, first, count, instancecount);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(DRAWARRAYS_INSTANCED);
    Serialise_glDrawArraysInstanced(mode, first, count, instancecount);

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

bool WrappedGLES::Serialise_glDrawArraysInstancedBaseInstanceEXT(GLenum mode, GLint first,
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
    Compat_glDrawArraysInstancedBaseInstanceEXT(Mode, First, Count, InstanceCount, BaseInstance);
    clearLocalDataBuffers();
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);
    string name = "glDrawArraysInstancedBaseInstance(" + ToStr::Get(Count) + ", " +
                  ToStr::Get(InstanceCount) + ")";

    FetchDrawcall draw;
    draw.name = name;
    draw.numIndices = Count;
    draw.numInstances = InstanceCount;
    draw.indexOffset = 0;
    draw.vertexOffset = First;
    draw.instanceOffset = BaseInstance;

    draw.flags |= eDraw_Drawcall | eDraw_Instanced;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);

    AddDrawcall(draw, true);
  }

  return true;
}

void WrappedGLES::glDrawArraysInstancedBaseInstanceEXT(GLenum mode, GLint first, GLsizei count,
                                                      GLsizei instancecount, GLuint baseinstance)
{
  CoherentMapImplicitBarrier();

  Compat_glDrawArraysInstancedBaseInstanceEXT(mode, first, count, instancecount, baseinstance);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(DRAWARRAYS_INSTANCEDBASEINSTANCE);
    Serialise_glDrawArraysInstancedBaseInstanceEXT(mode, first, count, instancecount, baseinstance);

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

bool WrappedGLES::Check_preElements()
{
  GLint idxbuf = 0;
  m_Real.glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, &idxbuf);

  if(idxbuf == 0)
  {
    AddDebugMessage(eDbgCategory_Undefined, eDbgSeverity_High, eDbgSource_IncorrectAPIUse,
                    "No index buffer bound at indexed draw!.");
    return false;
  }

  return true;
}

byte *WrappedGLES::Common_preElements(GLsizei Count, GLenum Type, uint64_t &IdxOffset)
{
  GLint idxbuf = 0;
  // while writing, check to see if an index buffer is bound
  if(m_State >= WRITING)
    m_Real.glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, &idxbuf);

  // serialise whether we're reading indices as memory
  SERIALISE_ELEMENT(bool, IndicesFromMemory, idxbuf == 0);

  if(IndicesFromMemory)
  {
    uint32_t IdxSize = Type == eGL_UNSIGNED_BYTE ? 1 : Type == eGL_UNSIGNED_SHORT
                                                           ? 2
                                                           : /*Type == eGL_UNSIGNED_INT*/ 4;

    // serialise the actual data (IdxOffset is a pointer not an offset in this case)
    SERIALISE_ELEMENT_BUF(byte *, idxdata, (void *)IdxOffset, size_t(IdxSize * Count));

    if(m_State <= EXECUTING)
    {
      GLsizeiptr idxlen = GLsizeiptr(IdxSize * Count);

      // resize fake index buffer if necessary
      if(idxlen > m_FakeIdxSize)
      {
        m_Real.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, 0);
        m_Real.glDeleteBuffers(1, &m_FakeIdxBuf);

        m_FakeIdxSize = idxlen;

        m_Real.glGenBuffers(1, &m_FakeIdxBuf);
        m_Real.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, m_FakeIdxBuf);
        Compat_glBufferStorageEXT(eGL_ELEMENT_ARRAY_BUFFER, m_FakeIdxSize, NULL, eGL_DYNAMIC_STORAGE_BIT_EXT);
      }

      // bind and update fake index buffer, to draw from the 'immediate' index data
      m_Real.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, m_FakeIdxBuf);

      m_Real.glBufferSubData(eGL_ELEMENT_ARRAY_BUFFER, 0, idxlen, idxdata);

      // Set offset to 0 - means we read data from start of our fake index buffer
      IdxOffset = 0;

      // we'll delete this later (only when replaying)
      return idxdata;
    }

    // can just return NULL, since we don't need to do any cleanup or deletion
  }

  return NULL;
}

void WrappedGLES::Common_postElements(byte *idxDelete)
{
  // unbind temporary fake index buffer we used to pass 'immediate' index data
  if(idxDelete)
  {
    m_Real.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, 0);

//    AddDebugMessage(eDbgCategory_Deprecated, eDbgSeverity_High, eDbgSource_IncorrectAPIUse,
//                    "Assuming GL core profile is used then specifying indices as a raw array, "
//                    "not as offset into element array buffer, is illegal.");

    // delete serialised data
    SAFE_DELETE_ARRAY(idxDelete);
  }
  clearLocalDataBuffers();
}

bool WrappedGLES::Serialise_glDrawElements(GLenum mode, GLsizei count, GLenum type,
                                             const void *indices)
{
  SERIALISE_ELEMENT(GLenum, Mode, mode);
  SERIALISE_ELEMENT(uint32_t, Count, count);
  SERIALISE_ELEMENT(GLenum, Type, type);
  SERIALISE_ELEMENT(uint64_t, IdxOffset, (uint64_t)indices);

  byte *idxDelete = Common_preElements(Count, Type, IdxOffset);

  if(m_State <= EXECUTING)
  {
    if(Check_preElements())
      m_Real.glDrawElements(Mode, Count, Type, (const void *)IdxOffset);

    Common_postElements(idxDelete);
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

    FetchDrawcall draw;
    draw.name = name;
    draw.numIndices = Count;
    draw.numInstances = 1;
    draw.indexOffset = uint32_t(IdxOffset) / IdxSize;
    draw.vertexOffset = 0;
    draw.instanceOffset = 0;

    draw.flags |= eDraw_Drawcall | eDraw_UseIBuffer;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);
    draw.indexByteWidth = IdxSize;

    AddDrawcall(draw, true);
  }

  return true;
}

void WrappedGLES::glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices)
{
  CoherentMapImplicitBarrier();

  m_Real.glDrawElements(mode, count, type, indices);

  if(m_State == WRITING_CAPFRAME)
  {
    writeFakeVertexAttribPointer(count);

    SCOPED_SERIALISE_CONTEXT(DRAWELEMENTS);
    Serialise_glDrawElements(mode, count, type, indices);

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

bool WrappedGLES::Serialise_glDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect)
{
  SERIALISE_ELEMENT(GLenum, Mode, mode);
  SERIALISE_ELEMENT(GLenum, Type, type);
  SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)indirect);

  if(m_State <= EXECUTING)
  {
    m_Real.glDrawElementsIndirect(Mode, Type, (const void *)Offset);
    clearLocalDataBuffers();
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    DrawElementsIndirectCommand params;
    Compat_glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, (GLintptr)Offset, sizeof(params), &params);

    AddEvent(desc);
    string name = "glDrawElementsIndirect(" + ToStr::Get(params.count) + ", " +
                  ToStr::Get(params.instanceCount) + ">)";

    uint32_t IdxSize = Type == eGL_UNSIGNED_BYTE ? 1 : Type == eGL_UNSIGNED_SHORT
                                                           ? 2
                                                           : /*Type == eGL_UNSIGNED_INT*/ 4;

    FetchDrawcall draw;
    draw.name = name;
    draw.numIndices = params.count;
    draw.numInstances = params.instanceCount;
    draw.indexOffset = params.firstIndex;
    draw.baseVertex = params.baseVertex;
    draw.instanceOffset = params.baseInstance;

    draw.flags |= eDraw_Drawcall | eDraw_UseIBuffer | eDraw_Instanced | eDraw_Indirect;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);
    draw.indexByteWidth = IdxSize;

    AddDrawcall(draw, true);

    MarkIndirectBufferUsage();
  }

  return true;
}

void WrappedGLES::glDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect)
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

bool WrappedGLES::Serialise_glDrawRangeElements(GLenum mode, GLuint start, GLuint end,
                                                  GLsizei count, GLenum type, const void *indices)
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
    if(Check_preElements())
      m_Real.glDrawRangeElements(Mode, Start, End, Count, Type, (const void *)IdxOffset);

    Common_postElements(idxDelete);
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

    FetchDrawcall draw;
    draw.name = name;
    draw.numIndices = Count;
    draw.numInstances = 1;
    draw.indexOffset = uint32_t(IdxOffset) / IdxSize;
    draw.vertexOffset = 0;
    draw.instanceOffset = 0;

    draw.flags |= eDraw_Drawcall | eDraw_UseIBuffer;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);
    draw.indexByteWidth = IdxSize;

    AddDrawcall(draw, true);
  }

  return true;
}

void WrappedGLES::glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count,
                                        GLenum type, const void *indices)
{
  CoherentMapImplicitBarrier();

  m_Real.glDrawRangeElements(mode, start, end, count, type, indices);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(DRAWRANGEELEMENTS);
    Serialise_glDrawRangeElements(mode, start, end, count, type, indices);

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

bool WrappedGLES::Serialise_glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end,
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

  byte *idxDelete = Common_preElements(Count, Type, IdxOffset);

  if(m_State <= EXECUTING)
  {
    if(Check_preElements())
      m_Real.glDrawRangeElementsBaseVertex(Mode, Start, End, Count, Type, (const void *)IdxOffset,
                                           BaseVtx);

    Common_postElements(idxDelete);
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

    FetchDrawcall draw;
    draw.name = name;
    draw.numIndices = Count;
    draw.numInstances = 1;
    draw.indexOffset = uint32_t(IdxOffset) / IdxSize;
    draw.baseVertex = BaseVtx;
    draw.instanceOffset = 0;

    draw.flags |= eDraw_Drawcall | eDraw_UseIBuffer;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);
    draw.indexByteWidth = IdxSize;

    AddDrawcall(draw, true);
  }

  return true;
}

void WrappedGLES::glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end,
                                                  GLsizei count, GLenum type, const void *indices,
                                                  GLint basevertex)
{
  CoherentMapImplicitBarrier();

  m_Real.glDrawRangeElementsBaseVertex(mode, start, end, count, type, indices, basevertex);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(DRAWRANGEELEMENTSBASEVERTEX);
    Serialise_glDrawRangeElementsBaseVertex(mode, start, end, count, type, indices, basevertex);

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

bool WrappedGLES::Serialise_glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type,
                                                       const void *indices, GLint basevertex)
{
  SERIALISE_ELEMENT(GLenum, Mode, mode);
  SERIALISE_ELEMENT(uint32_t, Count, count);
  SERIALISE_ELEMENT(GLenum, Type, type);
  SERIALISE_ELEMENT(uint64_t, IdxOffset, (uint64_t)indices);
  SERIALISE_ELEMENT(int32_t, BaseVtx, basevertex);

  byte *idxDelete = Common_preElements(Count, Type, IdxOffset);

  if(m_State <= EXECUTING)
  {
    if(Check_preElements())
      m_Real.glDrawElementsBaseVertex(Mode, Count, Type, (const void *)IdxOffset, BaseVtx);

    Common_postElements(idxDelete);
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

    FetchDrawcall draw;
    draw.name = name;
    draw.numIndices = Count;
    draw.numInstances = 1;
    draw.indexOffset = uint32_t(IdxOffset) / IdxSize;
    draw.baseVertex = BaseVtx;
    draw.instanceOffset = 0;

    draw.flags |= eDraw_Drawcall | eDraw_UseIBuffer;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);
    draw.indexByteWidth = IdxSize;

    AddDrawcall(draw, true);
  }

  return true;
}

void WrappedGLES::glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type,
                                             const void *indices, GLint basevertex)
{
  CoherentMapImplicitBarrier();

  m_Real.glDrawElementsBaseVertex(mode, count, type, indices, basevertex);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(DRAWELEMENTS_BASEVERTEX);
    Serialise_glDrawElementsBaseVertex(mode, count, type, indices, basevertex);

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

bool WrappedGLES::Serialise_glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type,
                                                      const void *indices, GLsizei instancecount)
{
  SERIALISE_ELEMENT(GLenum, Mode, mode);
  SERIALISE_ELEMENT(uint32_t, Count, count);
  SERIALISE_ELEMENT(GLenum, Type, type);
  SERIALISE_ELEMENT(uint64_t, IdxOffset, (uint64_t)indices);
  SERIALISE_ELEMENT(uint32_t, InstCount, instancecount);

  byte *idxDelete = Common_preElements(Count, Type, IdxOffset);

  if(m_State <= EXECUTING)
  {
    if(Check_preElements())
      m_Real.glDrawElementsInstanced(Mode, Count, Type, (const void *)IdxOffset, InstCount);

    Common_postElements(idxDelete);
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

    FetchDrawcall draw;
    draw.name = name;
    draw.numIndices = Count;
    draw.numInstances = InstCount;
    draw.indexOffset = uint32_t(IdxOffset) / IdxSize;
    draw.vertexOffset = 0;
    draw.instanceOffset = 0;

    draw.flags |= eDraw_Drawcall | eDraw_UseIBuffer;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);
    draw.indexByteWidth = IdxSize;

    AddDrawcall(draw, true);
  }

  return true;
}

void WrappedGLES::glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type,
                                            const void *indices, GLsizei instancecount)
{
  CoherentMapImplicitBarrier();

  m_Real.glDrawElementsInstanced(mode, count, type, indices, instancecount);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(DRAWELEMENTS_INSTANCED);
    Serialise_glDrawElementsInstanced(mode, count, type, indices, instancecount);

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

bool WrappedGLES::Serialise_glDrawElementsInstancedBaseInstanceEXT(GLenum mode, GLsizei count,
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

  byte *idxDelete = Common_preElements(Count, Type, IdxOffset);

  if(m_State <= EXECUTING)
  {
    if(Check_preElements())
      Compat_glDrawElementsInstancedBaseInstanceEXT(Mode, Count, Type, (const void *)IdxOffset,
                                                 InstCount, BaseInstance);

    Common_postElements(idxDelete);
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

    FetchDrawcall draw;
    draw.name = name;
    draw.numIndices = Count;
    draw.numInstances = InstCount;
    draw.indexOffset = uint32_t(IdxOffset) / IdxSize;
    draw.vertexOffset = 0;
    draw.instanceOffset = BaseInstance;

    draw.flags |= eDraw_Drawcall | eDraw_UseIBuffer;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);
    draw.indexByteWidth = IdxSize;

    AddDrawcall(draw, true);
  }

  return true;
}

void WrappedGLES::glDrawElementsInstancedBaseInstanceEXT(GLenum mode, GLsizei count, GLenum type,
                                                        const void *indices, GLsizei instancecount,
                                                        GLuint baseinstance)
{
  CoherentMapImplicitBarrier();

  Compat_glDrawElementsInstancedBaseInstanceEXT(mode, count, type, indices, instancecount, baseinstance);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(DRAWELEMENTS_INSTANCEDBASEINSTANCE);
    Serialise_glDrawElementsInstancedBaseInstanceEXT(mode, count, type, indices, instancecount,
                                                  baseinstance);

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

bool WrappedGLES::Serialise_glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count,
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

  byte *idxDelete = Common_preElements(Count, Type, IdxOffset);

  if(m_State <= EXECUTING)
  {
    if(Check_preElements())
      m_Real.glDrawElementsInstancedBaseVertex(Mode, Count, Type, (const void *)IdxOffset,
                                               InstCount, BaseVertex);

    Common_postElements(idxDelete);
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

    FetchDrawcall draw;
    draw.name = name;
    draw.numIndices = Count;
    draw.numInstances = InstCount;
    draw.indexOffset = uint32_t(IdxOffset) / IdxSize;
    draw.baseVertex = BaseVertex;
    draw.instanceOffset = 0;

    draw.flags |= eDraw_Drawcall | eDraw_UseIBuffer;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);
    draw.indexByteWidth = IdxSize;

    AddDrawcall(draw, true);
  }

  return true;
}

void WrappedGLES::glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type,
                                                      const void *indices, GLsizei instancecount,
                                                      GLint basevertex)
{
  CoherentMapImplicitBarrier();

  m_Real.glDrawElementsInstancedBaseVertex(mode, count, type, indices, instancecount, basevertex);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(DRAWELEMENTS_INSTANCEDBASEVERTEX);
    Serialise_glDrawElementsInstancedBaseVertex(mode, count, type, indices, instancecount,
                                                basevertex);

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

bool WrappedGLES::Serialise_glDrawElementsInstancedBaseVertexBaseInstanceEXT(
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

  byte *idxDelete = Common_preElements(Count, Type, IdxOffset);

  if(m_State <= EXECUTING)
  {
    if(Check_preElements())
      Compat_glDrawElementsInstancedBaseVertexBaseInstanceEXT(
          Mode, Count, Type, (const void *)IdxOffset, InstCount, BaseVertex, BaseInstance);

    Common_postElements(idxDelete);
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

    FetchDrawcall draw;
    draw.name = name;
    draw.numIndices = Count;
    draw.numInstances = InstCount;
    draw.indexOffset = uint32_t(IdxOffset) / IdxSize;
    draw.baseVertex = BaseVertex;
    draw.instanceOffset = BaseInstance;

    draw.flags |= eDraw_Drawcall | eDraw_UseIBuffer;

    draw.topology = MakePrimitiveTopology(m_Real, Mode);
    draw.indexByteWidth = IdxSize;

    AddDrawcall(draw, true);
  }

  return true;
}

void WrappedGLES::glDrawElementsInstancedBaseVertexBaseInstanceEXT(GLenum mode, GLsizei count,
                                                                  GLenum type, const void *indices,
                                                                  GLsizei instancecount,
                                                                  GLint basevertex,
                                                                  GLuint baseinstance)
{
  CoherentMapImplicitBarrier();

  Compat_glDrawElementsInstancedBaseVertexBaseInstanceEXT(mode, count, type, indices, instancecount,
                                                       basevertex, baseinstance);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(DRAWELEMENTS_INSTANCEDBASEVERTEXBASEINSTANCE);
    Serialise_glDrawElementsInstancedBaseVertexBaseInstanceEXT(
        mode, count, type, indices, instancecount, basevertex, baseinstance);

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

// TODO pantos
//glMultiDrawArraysEXT
//glMultiDrawArraysIndirectEXT
//glMultiDrawElementsEXT
//glMultiDrawElementsBaseVertexEXT
//glMultiDrawElementsBaseVertexOES
//glMultiDrawElementsIndirectEXT

//bool WrappedGLES::Serialise_glMultiDrawArrays(GLenum mode, const GLint *first,
//                                                const GLsizei *count, GLsizei drawcount)
//{
//  SERIALISE_ELEMENT(GLenum, Mode, mode);
//  SERIALISE_ELEMENT(uint32_t, Count, drawcount);
//
//  SERIALISE_ELEMENT_ARR(int32_t, firstArray, first, Count);
//  SERIALISE_ELEMENT_ARR(int32_t, countArray, count, Count);
//
//  if(m_State == READING)
//  {
//    m_Real.glMultiDrawArrays(Mode, firstArray, countArray, Count);
//  }
//  else if(m_State <= EXECUTING)
//  {
//    size_t i = 0;
//    for(; i < m_Events.size(); i++)
//    {
//      if(m_Events[i].eventID >= m_CurEventID)
//        break;
//    }
//
//    while(i > 1 && m_Events[i - 1].fileOffset == m_Events[i].fileOffset)
//      i--;
//
//    uint32_t baseEventID = m_Events[i].eventID;
//
//    if(m_LastEventID < baseEventID)
//    {
//      // To add the multidraw, we made an event N that is the 'parent' marker, then
//      // N+1, N+2, N+3, ... for each of the sub-draws. If the first sub-draw is selected
//      // then we'll replay up to N but not N+1, so just do nothing - we DON'T want to draw
//      // the first sub-draw in that range.
//    }
//    else if(m_FirstEventID <= baseEventID && m_LastEventID >= baseEventID)
//    {
//      // if we're replaying part-way into a multidraw, we can replay the first part 'easily'
//      // by just reducing the Count parameter to however many we want to replay. This only
//      // works if we're replaying from the first multidraw to the nth (n less than Count)
//      m_Real.glMultiDrawArrays(Mode, firstArray, countArray,
//                               RDCMIN(Count, m_LastEventID - baseEventID + 1));
//    }
//    else
//    {
//      // otherwise we do the 'hard' case, draw only one multidraw
//      // note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
//      // a single draw.
//      RDCASSERT(m_LastEventID == m_FirstEventID);
//
//      uint32_t drawidx = (m_LastEventID - baseEventID);
//
//      m_Real.glDrawArrays(Mode, firstArray[drawidx], countArray[drawidx]);
//    }
//  }
//
//  const string desc = m_pSerialiser->GetDebugStr();
//
//  Serialise_DebugMessages();
//
//  if(m_State == READING)
//  {
//    string name = "glMultiDrawArrays(" + ToStr::Get(Count) + ")";
//
//    FetchDrawcall draw;
//    draw.name = name;
//    draw.flags |= eDraw_MultiDraw;
//
//    draw.topology = MakePrimitiveTopology(m_Real, Mode);
//
//    AddDrawcall(draw, false);
//
//    m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());
//
//    m_CurEventID++;
//
//    for(uint32_t i = 0; i < Count; i++)
//    {
//      FetchDrawcall multidraw;
//      multidraw.numIndices = countArray[i];
//      multidraw.vertexOffset = firstArray[i];
//
//      multidraw.name =
//          "glMultiDrawArrays[" + ToStr::Get(i) + "](" + ToStr::Get(multidraw.numIndices) + ")";
//
//      multidraw.flags |= eDraw_Drawcall;
//
//      multidraw.topology = MakePrimitiveTopology(m_Real, Mode);
//
//      AddEvent(desc);
//      AddDrawcall(multidraw, true);
//
//      m_CurEventID++;
//    }
//
//    m_DrawcallStack.pop_back();
//  }
//  else
//  {
//    m_CurEventID += Count + 1;
//  }
//
//  SAFE_DELETE_ARRAY(firstArray);
//  SAFE_DELETE_ARRAY(countArray);
//
//  return true;
//}
//
//void WrappedGLES::glMultiDrawArrays(GLenum mode, const GLint *first, const GLsizei *count,
//                                      GLsizei drawcount)
//{
//  CoherentMapImplicitBarrier();
//
//  m_Real.glMultiDrawArrays(mode, first, count, drawcount);
//
//  if(m_State == WRITING_CAPFRAME)
//  {
//    SCOPED_SERIALISE_CONTEXT(MULTI_DRAWARRAYS);
//    Serialise_glMultiDrawArrays(mode, first, count, drawcount);
//
//    m_ContextRecord->AddChunk(scope.Get());
//
//    GLRenderState state(&m_Real, m_pSerialiser, m_State);
//    state.FetchState(GetCtx(), this);
//    state.MarkReferenced(this, false);
//  }
//  else if(m_State == WRITING_IDLE)
//  {
//    GLRenderState state(&m_Real, m_pSerialiser, m_State);
//    state.MarkDirty(this);
//  }
//}
//
//bool WrappedGLES::Serialise_glMultiDrawElements(GLenum mode, const GLsizei *count, GLenum type,
//                                                  const void *const *indices, GLsizei drawcount)
//{
//  SERIALISE_ELEMENT(GLenum, Mode, mode);
//  SERIALISE_ELEMENT(GLenum, Type, type);
//  SERIALISE_ELEMENT(uint32_t, Count, drawcount);
//
//  SERIALISE_ELEMENT_ARR(int32_t, countArray, count, Count);
//
//  void **idxOffsArray = new void *[Count];
//
//  // serialise pointer array as uint64s
//  if(m_State >= WRITING)
//  {
//    for(uint32_t i = 0; i < Count; i++)
//    {
//      uint64_t ptr = (uint64_t)indices[i];
//      m_pSerialiser->Serialise("idxOffsArray", ptr);
//    }
//  }
//  else
//  {
//    for(uint32_t i = 0; i < Count; i++)
//    {
//      uint64_t ptr = 0;
//      m_pSerialiser->Serialise("idxOffsArray", ptr);
//      idxOffsArray[i] = (void *)ptr;
//    }
//  }
//
//  if(m_State == READING)
//  {
//    m_Real.glMultiDrawElements(Mode, countArray, Type, idxOffsArray, Count);
//  }
//  else if(m_State <= EXECUTING)
//  {
//    size_t i = 0;
//    for(; i < m_Events.size(); i++)
//    {
//      if(m_Events[i].eventID >= m_CurEventID)
//        break;
//    }
//
//    while(i > 1 && m_Events[i - 1].fileOffset == m_Events[i].fileOffset)
//      i--;
//
//    uint32_t baseEventID = m_Events[i].eventID;
//
//    if(m_LastEventID < baseEventID)
//    {
//      // To add the multidraw, we made an event N that is the 'parent' marker, then
//      // N+1, N+2, N+3, ... for each of the sub-draws. If the first sub-draw is selected
//      // then we'll replay up to N but not N+1, so just do nothing - we DON'T want to draw
//      // the first sub-draw in that range.
//    }
//    else if(m_FirstEventID <= baseEventID && m_LastEventID >= baseEventID)
//    {
//      // if we're replaying part-way into a multidraw, we can replay the first part 'easily'
//      // by just reducing the Count parameter to however many we want to replay. This only
//      // works if we're replaying from the first multidraw to the nth (n less than Count)
//      m_Real.glMultiDrawElements(Mode, countArray, Type, idxOffsArray,
//                                 RDCMIN(Count, m_LastEventID - baseEventID + 1));
//    }
//    else
//    {
//      // otherwise we do the 'hard' case, draw only one multidraw
//      // note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
//      // a single draw.
//      RDCASSERT(m_LastEventID == m_FirstEventID);
//
//      uint32_t drawidx = (m_LastEventID - baseEventID);
//
//      m_Real.glDrawElements(Mode, countArray[drawidx], Type, idxOffsArray[drawidx]);
//    }
//  }
//
//  const string desc = m_pSerialiser->GetDebugStr();
//
//  Serialise_DebugMessages();
//
//  if(m_State == READING)
//  {
//    string name = "glMultiDrawElements(" + ToStr::Get(Count) + ")";
//
//    uint32_t IdxSize = Type == eGL_UNSIGNED_BYTE ? 1 : Type == eGL_UNSIGNED_SHORT
//                                                           ? 2
//                                                           : /*Type == eGL_UNSIGNED_INT*/ 4;
//
//    FetchDrawcall draw;
//    draw.name = name;
//
//    draw.flags |= eDraw_MultiDraw;
//    draw.indexByteWidth = IdxSize;
//    draw.numIndices = 0;
//
//    draw.topology = MakePrimitiveTopology(m_Real, Mode);
//
//    AddDrawcall(draw, false);
//
//    m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());
//
//    m_CurEventID++;
//
//    for(uint32_t i = 0; i < Count; i++)
//    {
//      FetchDrawcall multidraw;
//      multidraw.numIndices = countArray[i];
//      multidraw.indexOffset = (uint32_t)uint64_t(idxOffsArray[i]) & 0xFFFFFFFF;
//      multidraw.indexByteWidth = IdxSize;
//
//      multidraw.indexOffset /= IdxSize;
//
//      multidraw.name =
//          "glMultiDrawElements[" + ToStr::Get(i) + "](" + ToStr::Get(multidraw.numIndices) + ")";
//
//      multidraw.flags |= eDraw_Drawcall | eDraw_UseIBuffer;
//
//      multidraw.topology = MakePrimitiveTopology(m_Real, Mode);
//
//      AddEvent(desc);
//      AddDrawcall(multidraw, true);
//
//      m_CurEventID++;
//    }
//
//    m_DrawcallStack.pop_back();
//  }
//  else
//  {
//    m_CurEventID += Count + 1;
//  }
//
//  SAFE_DELETE_ARRAY(countArray);
//  SAFE_DELETE_ARRAY(idxOffsArray);
//
//  return true;
//}
//
//void WrappedGLES::glMultiDrawElements(GLenum mode, const GLsizei *count, GLenum type,
//                                        const void *const *indices, GLsizei drawcount)
//{
//  CoherentMapImplicitBarrier();
//
//  m_Real.glMultiDrawElements(mode, count, type, indices, drawcount);
//
//  if(m_State == WRITING_CAPFRAME)
//  {
//    SCOPED_SERIALISE_CONTEXT(MULTI_DRAWELEMENTS);
//    Serialise_glMultiDrawElements(mode, count, type, indices, drawcount);
//
//    m_ContextRecord->AddChunk(scope.Get());
//
//    GLRenderState state(&m_Real, m_pSerialiser, m_State);
//    state.FetchState(GetCtx(), this);
//    state.MarkReferenced(this, false);
//  }
//  else if(m_State == WRITING_IDLE)
//  {
//    GLRenderState state(&m_Real, m_pSerialiser, m_State);
//    state.MarkDirty(this);
//  }
//}
//
//bool WrappedGLES::Serialise_glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei *count,
//                                                            GLenum type, const void *const *indices,
//                                                            GLsizei drawcount,
//                                                            const GLint *basevertex)
//{
//  SERIALISE_ELEMENT(GLenum, Mode, mode);
//  SERIALISE_ELEMENT(GLenum, Type, type);
//  SERIALISE_ELEMENT(uint32_t, Count, drawcount);
//
//  SERIALISE_ELEMENT_ARR(int32_t, countArray, count, Count);
//  SERIALISE_ELEMENT_ARR(int32_t, baseArray, basevertex, Count);
//
//  void **idxOffsArray = new void *[Count];
//
//  // serialise pointer array as uint64s
//  if(m_State >= WRITING)
//  {
//    for(uint32_t i = 0; i < Count; i++)
//    {
//      uint64_t ptr = (uint64_t)indices[i];
//      m_pSerialiser->Serialise("idxOffsArray", ptr);
//    }
//  }
//  else
//  {
//    for(uint32_t i = 0; i < Count; i++)
//    {
//      uint64_t ptr = 0;
//      m_pSerialiser->Serialise("idxOffsArray", ptr);
//      idxOffsArray[i] = (void *)ptr;
//    }
//  }
//
//  if(m_State == READING)
//  {
//    m_Real.glMultiDrawElementsBaseVertex(Mode, countArray, Type, idxOffsArray, Count, baseArray);
//  }
//  else if(m_State <= EXECUTING)
//  {
//    size_t i = 0;
//    for(; i < m_Events.size(); i++)
//    {
//      if(m_Events[i].eventID >= m_CurEventID)
//        break;
//    }
//
//    while(i > 1 && m_Events[i - 1].fileOffset == m_Events[i].fileOffset)
//      i--;
//
//    uint32_t baseEventID = m_Events[i].eventID;
//
//    if(m_LastEventID < baseEventID)
//    {
//      // To add the multidraw, we made an event N that is the 'parent' marker, then
//      // N+1, N+2, N+3, ... for each of the sub-draws. If the first sub-draw is selected
//      // then we'll replay up to N but not N+1, so just do nothing - we DON'T want to draw
//      // the first sub-draw in that range.
//    }
//    else if(m_FirstEventID <= baseEventID && m_LastEventID >= baseEventID)
//    {
//      // if we're replaying part-way into a multidraw, we can replay the first part 'easily'
//      // by just reducing the Count parameter to however many we want to replay. This only
//      // works if we're replaying from the first multidraw to the nth (n less than Count)
//      m_Real.glMultiDrawElementsBaseVertex(Mode, countArray, Type, idxOffsArray,
//                                           RDCMIN(Count, m_LastEventID - baseEventID + 1), baseArray);
//    }
//    else
//    {
//      // otherwise we do the 'hard' case, draw only one multidraw
//      // note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
//      // a single draw.
//      RDCASSERT(m_LastEventID == m_FirstEventID);
//
//      uint32_t drawidx = (m_LastEventID - baseEventID);
//
//      m_Real.glDrawElementsBaseVertex(Mode, countArray[drawidx], Type, idxOffsArray[drawidx],
//                                      baseArray[drawidx]);
//    }
//  }
//
//  const string desc = m_pSerialiser->GetDebugStr();
//
//  Serialise_DebugMessages();
//
//  if(m_State == READING)
//  {
//    string name = "glMultiDrawElementsBaseVertex(" + ToStr::Get(Count) + ")";
//
//    uint32_t IdxSize = Type == eGL_UNSIGNED_BYTE ? 1 : Type == eGL_UNSIGNED_SHORT
//                                                           ? 2
//                                                           : /*Type == eGL_UNSIGNED_INT*/ 4;
//
//    FetchDrawcall draw;
//    draw.name = name;
//
//    draw.flags |= eDraw_MultiDraw;
//
//    draw.topology = MakePrimitiveTopology(m_Real, Mode);
//    draw.indexByteWidth = IdxSize;
//
//    AddDrawcall(draw, false);
//
//    m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());
//
//    m_CurEventID++;
//
//    for(uint32_t i = 0; i < Count; i++)
//    {
//      FetchDrawcall multidraw;
//      multidraw.numIndices = countArray[i];
//      multidraw.indexOffset = (uint32_t)uint64_t(idxOffsArray[i]) & 0xFFFFFFFF;
//      multidraw.baseVertex = baseArray[i];
//
//      multidraw.indexOffset /= IdxSize;
//
//      multidraw.name = "glMultiDrawElementsBaseVertex[" + ToStr::Get(i) + "](" +
//                       ToStr::Get(multidraw.numIndices) + ")";
//
//      multidraw.flags |= eDraw_Drawcall | eDraw_UseIBuffer;
//
//      multidraw.topology = MakePrimitiveTopology(m_Real, Mode);
//      multidraw.indexByteWidth = IdxSize;
//
//      AddEvent(desc);
//      AddDrawcall(multidraw, true);
//
//      m_CurEventID++;
//    }
//
//    m_DrawcallStack.pop_back();
//  }
//  else
//  {
//    m_CurEventID += Count + 1;
//  }
//
//  SAFE_DELETE_ARRAY(countArray);
//  SAFE_DELETE_ARRAY(baseArray);
//  SAFE_DELETE_ARRAY(idxOffsArray);
//
//  return true;
//}
//
//void WrappedGLES::glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei *count, GLenum type,
//                                                  const void *const *indices, GLsizei drawcount,
//                                                  const GLint *basevertex)
//{
//  CoherentMapImplicitBarrier();
//
//  m_Real.glMultiDrawElementsBaseVertex(mode, count, type, indices, drawcount, basevertex);
//
//  if(m_State == WRITING_CAPFRAME)
//  {
//    SCOPED_SERIALISE_CONTEXT(MULTI_DRAWELEMENTSBASEVERTEX);
//    Serialise_glMultiDrawElementsBaseVertex(mode, count, type, indices, drawcount, basevertex);
//
//    m_ContextRecord->AddChunk(scope.Get());
//
//    GLRenderState state(&m_Real, m_pSerialiser, m_State);
//    state.FetchState(GetCtx(), this);
//    state.MarkReferenced(this, false);
//  }
//  else if(m_State == WRITING_IDLE)
//  {
//    GLRenderState state(&m_Real, m_pSerialiser, m_State);
//    state.MarkDirty(this);
//  }
//}
//
//bool WrappedGLES::Serialise_glMultiDrawArraysIndirect(GLenum mode, const void *indirect,
//                                                        GLsizei drawcount, GLsizei stride)
//{
//  SERIALISE_ELEMENT(GLenum, Mode, mode);
//  SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)indirect);
//  SERIALISE_ELEMENT(uint32_t, Count, drawcount);
//  SERIALISE_ELEMENT(uint32_t, Stride, stride);
//
//  if(m_State == READING)
//  {
//    m_Real.glMultiDrawArraysIndirect(Mode, (const void *)Offset, Count, Stride);
//  }
//  else if(m_State <= EXECUTING)
//  {
//    size_t i = 0;
//    for(; i < m_Events.size(); i++)
//    {
//      if(m_Events[i].eventID >= m_CurEventID)
//        break;
//    }
//
//    while(i > 1 && m_Events[i - 1].fileOffset == m_Events[i].fileOffset)
//      i--;
//
//    uint32_t baseEventID = m_Events[i].eventID;
//
//    if(m_LastEventID < baseEventID)
//    {
//      // To add the multidraw, we made an event N that is the 'parent' marker, then
//      // N+1, N+2, N+3, ... for each of the sub-draws. If the first sub-draw is selected
//      // then we'll replay up to N but not N+1, so just do nothing - we DON'T want to draw
//      // the first sub-draw in that range.
//    }
//    else if(m_FirstEventID <= baseEventID && m_LastEventID >= baseEventID)
//    {
//      // if we're replaying part-way into a multidraw, we can replay the first part 'easily'
//      // by just reducing the Count parameter to however many we want to replay. This only
//      // works if we're replaying from the first multidraw to the nth (n less than Count)
//      m_Real.glMultiDrawArraysIndirect(Mode, (const void *)Offset,
//                                       RDCMIN(Count, m_LastEventID - baseEventID + 1), Stride);
//    }
//    else
//    {
//      // otherwise we do the 'hard' case, draw only one multidraw
//      // note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
//      // a single draw.
//      RDCASSERT(m_LastEventID == m_FirstEventID);
//
//      uint32_t drawidx = (m_LastEventID - baseEventID);
//
//      DrawArraysIndirectCommand params;
//
//      GLintptr offs = (GLintptr)Offset;
//      if(Stride != 0)
//        offs += Stride * drawidx;
//      else
//        offs += sizeof(params) * drawidx;
//
//      m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);
//
//      m_Real.glDrawArraysInstancedBaseInstance(Mode, params.first, params.count,
//                                               params.instanceCount, params.baseInstance);
//    }
//  }
//
//  const string desc = m_pSerialiser->GetDebugStr();
//
//  Serialise_DebugMessages();
//
//  if(m_State == READING)
//  {
//    string name = "glMultiDrawArraysIndirect(" + ToStr::Get(Count) + ")";
//
//    FetchDrawcall draw;
//    draw.name = name;
//
//    draw.flags |= eDraw_MultiDraw;
//
//    draw.topology = MakePrimitiveTopology(m_Real, Mode);
//
//    AddDrawcall(draw, false);
//
//    m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());
//
//    m_CurEventID++;
//
//    GLintptr offs = (GLintptr)Offset;
//
//    for(uint32_t i = 0; i < Count; i++)
//    {
//      DrawArraysIndirectCommand params;
//
//      m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);
//
//      if(Stride)
//        offs += Stride;
//      else
//        offs += sizeof(params);
//
//      FetchDrawcall multidraw;
//      multidraw.numIndices = params.count;
//      multidraw.numInstances = params.instanceCount;
//      multidraw.vertexOffset = params.first;
//      multidraw.instanceOffset = params.baseInstance;
//
//      multidraw.name = "glMultiDrawArraysIndirect[" + ToStr::Get(i) + "](<" +
//                       ToStr::Get(multidraw.numIndices) + ", " +
//                       ToStr::Get(multidraw.numInstances) + ">)";
//
//      multidraw.flags |= eDraw_Drawcall | eDraw_Instanced | eDraw_Indirect;
//
//      multidraw.topology = MakePrimitiveTopology(m_Real, Mode);
//
//      AddEvent(desc);
//      AddDrawcall(multidraw, true);
//
//      m_CurEventID++;
//    }
//
//    m_DrawcallStack.pop_back();
//  }
//  else
//  {
//    m_CurEventID += Count + 1;
//  }
//
//  return true;
//}
//
//void WrappedGLES::glMultiDrawArraysIndirect(GLenum mode, const void *indirect, GLsizei drawcount,
//                                              GLsizei stride)
//{
//  CoherentMapImplicitBarrier();
//
//  m_Real.glMultiDrawArraysIndirect(mode, indirect, drawcount, stride);
//
//  if(m_State == WRITING_CAPFRAME)
//  {
//    SCOPED_SERIALISE_CONTEXT(MULTI_DRAWARRAYS_INDIRECT);
//    Serialise_glMultiDrawArraysIndirect(mode, indirect, drawcount, stride);
//
//    m_ContextRecord->AddChunk(scope.Get());
//
//    GLRenderState state(&m_Real, m_pSerialiser, m_State);
//    state.FetchState(GetCtx(), this);
//    state.MarkReferenced(this, false);
//  }
//  else if(m_State == WRITING_IDLE)
//  {
//    GLRenderState state(&m_Real, m_pSerialiser, m_State);
//    state.MarkDirty(this);
//  }
//}
//
//bool WrappedGLES::Serialise_glMultiDrawElementsIndirect(GLenum mode, GLenum type,
//                                                          const void *indirect, GLsizei drawcount,
//                                                          GLsizei stride)
//{
//  SERIALISE_ELEMENT(GLenum, Mode, mode);
//  SERIALISE_ELEMENT(GLenum, Type, type);
//  SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)indirect);
//  SERIALISE_ELEMENT(uint32_t, Count, drawcount);
//  SERIALISE_ELEMENT(uint32_t, Stride, stride);
//
//  uint32_t IdxSize = Type == eGL_UNSIGNED_BYTE ? 1 : Type == eGL_UNSIGNED_SHORT
//                                                         ? 2
//                                                         : /*Type == eGL_UNSIGNED_INT*/ 4;
//
//  if(m_State == READING)
//  {
//    m_Real.glMultiDrawElementsIndirect(Mode, Type, (const void *)Offset, Count, Stride);
//  }
//  else if(m_State <= EXECUTING)
//  {
//    size_t i = 0;
//    for(; i < m_Events.size(); i++)
//    {
//      if(m_Events[i].eventID >= m_CurEventID)
//        break;
//    }
//
//    while(i > 1 && m_Events[i - 1].fileOffset == m_Events[i].fileOffset)
//      i--;
//
//    uint32_t baseEventID = m_Events[i].eventID;
//
//    if(m_LastEventID < baseEventID)
//    {
//      // To add the multidraw, we made an event N that is the 'parent' marker, then
//      // N+1, N+2, N+3, ... for each of the sub-draws. If the first sub-draw is selected
//      // then we'll replay up to N but not N+1, so just do nothing - we DON'T want to draw
//      // the first sub-draw in that range.
//    }
//    else if(m_FirstEventID <= baseEventID && m_LastEventID >= baseEventID)
//    {
//      // if we're replaying part-way into a multidraw, we can replay the first part 'easily'
//      // by just reducing the Count parameter to however many we want to replay. This only
//      // works if we're replaying from the first multidraw to the nth (n less than Count)
//      m_Real.glMultiDrawElementsIndirect(Mode, Type, (const void *)Offset,
//                                         RDCMIN(Count, m_LastEventID - baseEventID + 1), Stride);
//    }
//    else
//    {
//      // otherwise we do the 'hard' case, draw only one multidraw
//      // note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
//      // a single draw.
//      RDCASSERT(m_LastEventID == m_FirstEventID);
//
//      uint32_t drawidx = (m_LastEventID - baseEventID);
//
//      DrawElementsIndirectCommand params;
//
//      GLintptr offs = (GLintptr)Offset;
//      if(Stride != 0)
//        offs += Stride * drawidx;
//      else
//        offs += sizeof(params) * drawidx;
//
//      m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);
//
//      m_Real.glDrawElementsInstancedBaseVertexBaseInstance(
//          Mode, params.count, Type, (const void *)ptrdiff_t(params.firstIndex * IdxSize),
//          params.instanceCount, params.baseVertex, params.baseInstance);
//    }
//  }
//
//  const string desc = m_pSerialiser->GetDebugStr();
//
//  Serialise_DebugMessages();
//
//  if(m_State == READING)
//  {
//    string name = "glMultiDrawElementsIndirect(" + ToStr::Get(Count) + ")";
//
//    FetchDrawcall draw;
//    draw.name = name;
//
//    draw.flags |= eDraw_MultiDraw;
//
//    draw.topology = MakePrimitiveTopology(m_Real, Mode);
//    draw.indexByteWidth = IdxSize;
//
//    AddDrawcall(draw, false);
//
//    m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());
//
//    m_CurEventID++;
//
//    GLintptr offs = (GLintptr)Offset;
//
//    for(uint32_t i = 0; i < Count; i++)
//    {
//      DrawElementsIndirectCommand params;
//
//      m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);
//
//      if(Stride)
//        offs += Stride;
//      else
//        offs += sizeof(params);
//
//      FetchDrawcall multidraw;
//      multidraw.numIndices = params.count;
//      multidraw.numInstances = params.instanceCount;
//      multidraw.indexOffset = params.firstIndex;
//      multidraw.baseVertex = params.baseVertex;
//      multidraw.instanceOffset = params.baseInstance;
//
//      multidraw.name = "glMultiDrawElementsIndirect[" + ToStr::Get(i) + "](<" +
//                       ToStr::Get(multidraw.numIndices) + ", " +
//                       ToStr::Get(multidraw.numInstances) + ">)";
//
//      multidraw.flags |= eDraw_Drawcall | eDraw_UseIBuffer | eDraw_Instanced | eDraw_Indirect;
//
//      multidraw.topology = MakePrimitiveTopology(m_Real, Mode);
//      multidraw.indexByteWidth = IdxSize;
//
//      AddEvent(desc);
//      AddDrawcall(multidraw, true);
//
//      m_CurEventID++;
//    }
//
//    m_DrawcallStack.pop_back();
//  }
//  else
//  {
//    m_CurEventID += Count + 1;
//  }
//
//  return true;
//}
//
//void WrappedGLES::glMultiDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect,
//                                                GLsizei drawcount, GLsizei stride)
//{
//  CoherentMapImplicitBarrier();
//
//  m_Real.glMultiDrawElementsIndirect(mode, type, indirect, drawcount, stride);
//
//  if(m_State == WRITING_CAPFRAME)
//  {
//    SCOPED_SERIALISE_CONTEXT(MULTI_DRAWELEMENTS_INDIRECT);
//    Serialise_glMultiDrawElementsIndirect(mode, type, indirect, drawcount, stride);
//
//    m_ContextRecord->AddChunk(scope.Get());
//
//    GLRenderState state(&m_Real, m_pSerialiser, m_State);
//    state.FetchState(GetCtx(), this);
//    state.MarkReferenced(this, false);
//  }
//  else if(m_State == WRITING_IDLE)
//  {
//    GLRenderState state(&m_Real, m_pSerialiser, m_State);
//    state.MarkDirty(this);
//  }
//}

bool WrappedGLES::Serialise_glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value)
{
  GLuint framebuffer = 0;

  if(m_State == WRITING_CAPFRAME)
  {
    if(GetCtxData().m_DrawFramebufferRecord)
      framebuffer = GetCtxData().m_DrawFramebufferRecord->Resource.name;
  }

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

    if(m_State <= EXECUTING)
    {
      SafeDrawFramebufferBinder safeDrawFramebufferBinder(m_Real, framebuffer);
      m_Real.glClearBufferfv(buf, drawbuf, &v.x);
    }
  }
  else
  {
    SERIALISE_ELEMENT(float, val, *value);

    if(m_State == READING)
      name = "glClearBufferfv(" + ToStr::Get(buf) + ", " + ToStr::Get(drawbuf) + ", " +
             ToStr::Get(val) + ")";

    if(m_State <= EXECUTING)
    {
      SafeDrawFramebufferBinder safeDrawFramebufferBinder(m_Real, framebuffer);
      m_Real.glClearBufferfv(buf, drawbuf, &val);
    }
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);

    FetchDrawcall draw;
    draw.name = name;
    draw.flags |= eDraw_Clear;
    if(buf == eGL_COLOR)
      draw.flags |= eDraw_ClearColour;
    else
      draw.flags |= eDraw_ClearDepthStencil;

    AddDrawcall(draw, true);

    GLuint attachment = 0;
    GLenum attachName =
        buf == eGL_COLOR ? GLenum(eGL_COLOR_ATTACHMENT0 + drawbuf) : eGL_DEPTH_ATTACHMENT;
    GLenum type = eGL_TEXTURE;

    SafeDrawFramebufferBinder safeDrawFramebufferBinder(m_Real, framebuffer);
    m_Real.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, attachName,
                                                 eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                 (GLint *)&attachment);
    m_Real.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, attachName,
                                                 eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                 (GLint *)&type);

    if(attachment)
    {
      if(type == eGL_TEXTURE)
        m_ResourceUses[GetResourceManager()->GetID(TextureRes(GetCtx(), attachment))].push_back(
            EventUsage(m_CurEventID, eUsage_Clear));
      else
        m_ResourceUses[GetResourceManager()->GetID(RenderbufferRes(GetCtx(), attachment))].push_back(
            EventUsage(m_CurEventID, eUsage_Clear));
    }
  }

  return true;
}

void WrappedGLES::glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value)
{
  CoherentMapImplicitBarrier();

  m_Real.glClearBufferfv(buffer, drawbuffer, value);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(CLEARBUFFERF);
    Serialise_glClearBufferfv(buffer, drawbuffer, value);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedGLES::Serialise_glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *value)
{
  GLuint framebuffer = 0;

  if(m_State == WRITING_CAPFRAME)
  {
    GLuint framebuffer = 0;
    if(GetCtxData().m_DrawFramebufferRecord)
      framebuffer = GetCtxData().m_DrawFramebufferRecord->Resource.name;
  }

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

    if(m_State <= EXECUTING)
    {
      SafeDrawFramebufferBinder safeDrawFramebufferBinder(m_Real, framebuffer);
      m_Real.glClearBufferiv(buf, drawbuf, v);
    }
  }
  else
  {
    SERIALISE_ELEMENT(int32_t, val, *value);

    if(m_State == READING)
      name = "glClearBufferiv(" + ToStr::Get(buf) + ", " + ToStr::Get(drawbuf) + ", " +
             ToStr::Get(val) + ")";

    if(m_State <= EXECUTING)
    {
      SafeDrawFramebufferBinder safeDrawFramebufferBinder(m_Real, framebuffer);
      m_Real.glClearBufferiv(buf, drawbuf, &val);
    }
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);

    FetchDrawcall draw;
    draw.name = name;
    draw.flags |= eDraw_Clear;
    if(buf == eGL_COLOR)
      draw.flags |= eDraw_ClearColour;
    else
      draw.flags |= eDraw_ClearDepthStencil;

    AddDrawcall(draw, true);

    GLuint attachment = 0;
    GLenum attachName =
        buf == eGL_COLOR ? GLenum(eGL_COLOR_ATTACHMENT0 + drawbuf) : eGL_STENCIL_ATTACHMENT;
    GLenum type = eGL_TEXTURE;

    SafeDrawFramebufferBinder safeDrawFramebufferBinder(m_Real, framebuffer);
    m_Real.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, attachName,
                                                 eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                 (GLint *)&attachment);
    m_Real.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, attachName,
                                                 eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                 (GLint *)&type);

    if(attachment)
    {
      if(type == eGL_TEXTURE)
        m_ResourceUses[GetResourceManager()->GetID(TextureRes(GetCtx(), attachment))].push_back(
            EventUsage(m_CurEventID, eUsage_Clear));
      else
        m_ResourceUses[GetResourceManager()->GetID(RenderbufferRes(GetCtx(), attachment))].push_back(
            EventUsage(m_CurEventID, eUsage_Clear));
    }
  }

  return true;
}

void WrappedGLES::glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *value)
{
  CoherentMapImplicitBarrier();

  m_Real.glClearBufferiv(buffer, drawbuffer, value);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(CLEARBUFFERI);
    Serialise_glClearBufferiv(buffer, drawbuffer, value);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedGLES::Serialise_glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *value)
{
  GLuint framebuffer = 0;

  if(m_State == WRITING_CAPFRAME)
  {
    GLuint framebuffer = 0;
    if(GetCtxData().m_DrawFramebufferRecord)
      framebuffer = GetCtxData().m_DrawFramebufferRecord->Resource.name;
  }

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

    if(m_State <= EXECUTING)
    {
      SafeDrawFramebufferBinder safeDrawFramebufferBinder(m_Real, framebuffer);
      m_Real.glClearBufferuiv(buf, drawbuf, v);
    }
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);

    FetchDrawcall draw;
    draw.name = name;
    draw.flags |= eDraw_Clear | eDraw_ClearColour;

    AddDrawcall(draw, true);

    GLuint attachment = 0;
    GLenum attachName = GLenum(eGL_COLOR_ATTACHMENT0 + drawbuf);
    GLenum type = eGL_TEXTURE;

    SafeDrawFramebufferBinder safeDrawFramebufferBinder(m_Real, framebuffer);
    m_Real.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, attachName,
                                                 eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                 (GLint *)&attachment);
    m_Real.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, attachName,
                                                 eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                 (GLint *)&type);

    if(attachment)
    {
      if(type == eGL_TEXTURE)
        m_ResourceUses[GetResourceManager()->GetID(TextureRes(GetCtx(), attachment))].push_back(
            EventUsage(m_CurEventID, eUsage_Clear));
      else
        m_ResourceUses[GetResourceManager()->GetID(RenderbufferRes(GetCtx(), attachment))].push_back(
            EventUsage(m_CurEventID, eUsage_Clear));
    }
  }

  return true;
}

void WrappedGLES::glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *value)
{
  CoherentMapImplicitBarrier();

  m_Real.glClearBufferuiv(buffer, drawbuffer, value);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(CLEARBUFFERUI);
    Serialise_glClearBufferuiv(buffer, drawbuffer, value);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedGLES::Serialise_glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
  // drawBuffer must be zero
  drawbuffer = 0;

  GLuint framebuffer = 0;

  if(m_State == WRITING_CAPFRAME)
  {
    GLuint framebuffer = 0;
    if(GetCtxData().m_DrawFramebufferRecord)
      framebuffer = GetCtxData().m_DrawFramebufferRecord->Resource.name;
  }

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

  if(m_State <= EXECUTING)
  {
    SafeDrawFramebufferBinder safeDrawFramebufferBinder(m_Real, framebuffer);
    m_Real.glClearBufferfi(buf, drawbuffer, d, s);
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);
    string name = "glClearBufferfi(" + ToStr::Get(d) + ToStr::Get(s) + ")";

    FetchDrawcall draw;
    draw.name = name;
    draw.flags |= eDraw_Clear | eDraw_ClearDepthStencil;

    AddDrawcall(draw, true);

    GLuint attachment = 0;
    GLenum type = eGL_TEXTURE;

    SafeDrawFramebufferBinder safeDrawFramebufferBinder(m_Real, framebuffer);
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
            EventUsage(m_CurEventID, eUsage_Clear));
      else
        m_ResourceUses[GetResourceManager()->GetID(RenderbufferRes(GetCtx(), attachment))].push_back(
            EventUsage(m_CurEventID, eUsage_Clear));
    }

    attachment = 0;
    type = eGL_TEXTURE;

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
            EventUsage(m_CurEventID, eUsage_Clear));
      else
        m_ResourceUses[GetResourceManager()->GetID(RenderbufferRes(GetCtx(), attachment))].push_back(
            EventUsage(m_CurEventID, eUsage_Clear));
    }
  }

  return true;
}

void WrappedGLES::glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
  CoherentMapImplicitBarrier();

  m_Real.glClearBufferfi(buffer, drawbuffer, depth, stencil);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(CLEARBUFFERFI);
    Serialise_glClearBufferfi(buffer, drawbuffer, depth, stencil);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedGLES::Serialise_glClear(GLbitfield mask)
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

    FetchDrawcall draw;
    draw.name = name;
    draw.flags |= eDraw_Clear;
    if(Mask & GL_COLOR_BUFFER_BIT)
      draw.flags |= eDraw_ClearColour;
    if(Mask & (eGL_DEPTH_BUFFER_BIT | eGL_STENCIL_BUFFER_BIT))
      draw.flags |= eDraw_ClearDepthStencil;

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
              EventUsage(m_CurEventID, eUsage_Clear));
        else
          m_ResourceUses[GetResourceManager()->GetID(RenderbufferRes(GetCtx(), attachment))].push_back(
              EventUsage(m_CurEventID, eUsage_Clear));
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
              EventUsage(m_CurEventID, eUsage_Clear));
        else
          m_ResourceUses[GetResourceManager()->GetID(RenderbufferRes(GetCtx(), attachment))].push_back(
              EventUsage(m_CurEventID, eUsage_Clear));
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
                EventUsage(m_CurEventID, eUsage_Clear));
          else
            m_ResourceUses[GetResourceManager()->GetID(RenderbufferRes(GetCtx(), attachment))].push_back(
                EventUsage(m_CurEventID, eUsage_Clear));
        }
      }
    }
  }

  return true;
}

void WrappedGLES::glClear(GLbitfield mask)
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

bool WrappedGLES::Serialise_glPrimitiveBoundingBox (GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW,
                                                   GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW)
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

void WrappedGLES::glPrimitiveBoundingBox (GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW,
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
