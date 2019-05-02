/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

static std::string DecodeObjectLabel(GLsizei length, const GLchar *label)
{
  // we share implementations between KHR_debug and EXT_debug_label, however KHR_debug follows the
  // pattern elsewhere (e.g. in glShaderSource) of a length of -1 meaning indeterminate
  // NULL-terminated length, but EXT_debug_label takes length of 0 to mean that.
  GLsizei realLength = length;
  if(gl_CurChunk == GLChunk::glLabelObjectEXT && length == 0)
    realLength = -1;

  // if length is negative (after above twiddling), it's taken from strlen and the label must be
  // NULL-terminated
  if(realLength < 0)
    realLength = label ? (GLsizei)strlen(label) : 0;

  if(realLength == 0 || label == NULL)
    return "";

  return std::string(label, label + realLength);
}

static void ReturnObjectlabel(std::string name, GLsizei bufSize, GLsizei *length, GLchar *label)
{
  // If <label> is NULL and <length> is non-NULL then no string will be returned and the length
  // of the label will be returned in <length>.
  if(length && !label)
  {
    *length = (GLsizei)name.size();
  }
  else
  {
    // The maximum number of characters that may be written into <label>, including the null
    // terminator, is specified by <bufSize>.
    if(bufSize > 0)
    {
      name.resize(bufSize - 1);

      if(label)
        memcpy(label, name.c_str(), name.size() + 1);

      // The actual number of characters written into <label>, excluding the null terminator, is
      // returned in <length>.
      // If <length> is NULL, no length is returned.
      if(length)
        *length = (GLsizei)name.size();
    }
    else
    {
      if(length)
        *length = 0;
    }
  }
}

GLResource WrappedOpenGL::GetResource(GLenum identifier, GLuint name)
{
  GLResource Resource;

  switch(identifier)
  {
    case eGL_TEXTURE: Resource = TextureRes(GetCtx(), name); break;
    case eGL_BUFFER_OBJECT_EXT:
    case eGL_BUFFER: Resource = BufferRes(GetCtx(), name); break;
    case eGL_PROGRAM_OBJECT_EXT:
    case eGL_PROGRAM: Resource = ProgramRes(GetCtx(), name); break;
    case eGL_PROGRAM_PIPELINE_OBJECT_EXT:
    case eGL_PROGRAM_PIPELINE: Resource = ProgramPipeRes(GetCtx(), name); break;
    case eGL_VERTEX_ARRAY_OBJECT_EXT:
    case eGL_VERTEX_ARRAY: Resource = VertexArrayRes(GetCtx(), name); break;
    case eGL_SHADER_OBJECT_EXT:
    case eGL_SHADER: Resource = ShaderRes(GetCtx(), name); break;
    case eGL_QUERY_OBJECT_EXT:
    case eGL_QUERY: Resource = QueryRes(GetCtx(), name); break;
    case eGL_TRANSFORM_FEEDBACK: Resource = FeedbackRes(GetCtx(), name); break;
    case eGL_SAMPLER: Resource = SamplerRes(GetCtx(), name); break;
    case eGL_RENDERBUFFER: Resource = RenderbufferRes(GetCtx(), name); break;
    case eGL_FRAMEBUFFER: Resource = FramebufferRes(GetCtx(), name); break;
    default: RDCERR("Unhandled namespace in glObjectLabel");
  }

  return Resource;
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glObjectLabel(SerialiserType &ser, GLenum identifier, GLuint name,
                                            GLsizei length, const GLchar *label)
{
  GLResource Resource;
  std::string Label;

  if(ser.IsWriting())
  {
    Label = DecodeObjectLabel(length, label);

    Resource = GetResource(identifier, name);
  }

  SERIALISE_ELEMENT(Resource);
  SERIALISE_ELEMENT(length);
  SERIALISE_ELEMENT(Label);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && Resource.name)
  {
    ResourceId origId = GetResourceManager()->GetOriginalID(GetResourceManager()->GetID(Resource));

    GetResourceManager()->SetName(origId, Label);

    ResourceDescription &descr = GetReplay()->GetResourceDesc(origId);
    descr.SetCustomName(Label);
    AddResourceCurChunk(descr);
  }

  return true;
}

void WrappedOpenGL::glLabelObjectEXT(GLenum identifier, GLuint name, GLsizei length,
                                     const GLchar *label)
{
  if(GL.glLabelObjectEXT)
  {
    SERIALISE_TIME_CALL(GL.glLabelObjectEXT(identifier, name, length, label));
  }
  else
  {
    m_ScratchSerialiser.ChunkMetadata().timestampMicro = RenderDoc::Inst().GetMicrosecondTimestamp();
    m_ScratchSerialiser.ChunkMetadata().durationMicro = 0;
  }

  if(IsCaptureMode(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glObjectLabel(ser, identifier, name, length, label);

    GLResourceRecord *record = m_DeviceRecord;

    GLResource res = GetResource(identifier, name);

    if(GetResourceManager()->HasResourceRecord(res))
      record = GetResourceManager()->GetResourceRecord(res);

    GetResourceManager()->SetName(res, DecodeObjectLabel(length, label));

    record->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glObjectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar *label)
{
  if(GL.glObjectLabel)
  {
    SERIALISE_TIME_CALL(GL.glObjectLabel(identifier, name, length, label));
  }
  else
  {
    m_ScratchSerialiser.ChunkMetadata().timestampMicro = RenderDoc::Inst().GetMicrosecondTimestamp();
    m_ScratchSerialiser.ChunkMetadata().durationMicro = 0;
  }

  if(IsCaptureMode(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glObjectLabel(ser, identifier, name, length, label);

    GLResourceRecord *record = m_DeviceRecord;

    GLResource res = GetResource(identifier, name);

    if(GetResourceManager()->HasResourceRecord(res))
      record = GetResourceManager()->GetResourceRecord(res);

    GetResourceManager()->SetName(res, DecodeObjectLabel(length, label));

    record->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glObjectPtrLabel(const void *ptr, GLsizei length, const GLchar *label)
{
  if(GL.glObjectPtrLabel)
  {
    SERIALISE_TIME_CALL(GL.glObjectPtrLabel(ptr, length, label));
  }
  else
  {
    m_ScratchSerialiser.ChunkMetadata().timestampMicro = RenderDoc::Inst().GetMicrosecondTimestamp();
    m_ScratchSerialiser.ChunkMetadata().durationMicro = 0;
  }

  if(IsCaptureMode(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    ResourceId id = GetResourceManager()->GetSyncID((GLsync)ptr);
    Serialise_glObjectLabel(ser, eGL_SYNC_FENCE, GetResourceManager()->GetCurrentResource(id).name,
                            length, label);

    GetResourceManager()->SetName(id, DecodeObjectLabel(length, label));

    GetContextRecord()->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glDebugMessageCallback(GLDEBUGPROC callback, const void *userParam)
{
  GetCtxData().m_RealDebugFunc = callback;
  GetCtxData().m_RealDebugFuncParam = userParam;

  if(GL.glDebugMessageCallback)
  {
    GL.glDebugMessageCallback(&DebugSnoopStatic, this);
  }
}

void WrappedOpenGL::glDebugMessageControl(GLenum source, GLenum type, GLenum severity,
                                          GLsizei count, const GLuint *ids, GLboolean enabled)
{
  // we could exert control over debug messages here
  if(GL.glDebugMessageControl)
  {
    GL.glDebugMessageControl(source, type, severity, count, ids, enabled);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDebugMessageInsert(SerialiserType &ser, GLenum source, GLenum type,
                                                   GLuint id, GLenum severity, GLsizei length,
                                                   const GLchar *buf)
{
  std::string name = buf ? std::string(buf, buf + (length > 0 ? length : strlen(buf))) : "";

  // unused, just for the user's benefit
  SERIALISE_ELEMENT(source);
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT(id);
  SERIALISE_ELEMENT(severity);
  SERIALISE_ELEMENT(length);
  SERIALISE_ELEMENT(name);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(m_ReplayMarkers)
      GLMarkerRegion::Set(name);

    if(IsLoading(m_State))
    {
      DrawcallDescription draw;
      draw.name = name;
      draw.flags |= DrawFlags::SetMarker;

      AddEvent();
      AddDrawcall(draw, false);
    }
  }

  return true;
}

void WrappedOpenGL::HandleVRFrameMarkers(const GLchar *buf, GLsizei length)
{
  if(strstr(buf, "vr-marker,frame_end,type,application") != NULL)
  {
    SwapBuffers((void *)m_ActiveContexts[Threading::GetCurrentID()].wnd);
    m_UsesVRMarkers = true;

    if(IsActiveCapturing(m_State))
    {
      m_AcceptedCtx.clear();
      m_AcceptedCtx.insert(GetCtx().ctx);
      RDCDEBUG("Only resource ID accepted is %llu", GetCtxData().m_ContextDataResourceID);
    }
  }
}

void WrappedOpenGL::glDebugMessageInsert(GLenum source, GLenum type, GLuint id, GLenum severity,
                                         GLsizei length, const GLchar *buf)
{
  if(GL.glDebugMessageInsert)
  {
    SERIALISE_TIME_CALL(GL.glDebugMessageInsert(source, type, id, severity, length, buf));
  }
  else
  {
    m_ScratchSerialiser.ChunkMetadata().timestampMicro = RenderDoc::Inst().GetMicrosecondTimestamp();
    m_ScratchSerialiser.ChunkMetadata().durationMicro = 0;
  }

  HandleVRFrameMarkers(buf, length);

  if(IsActiveCapturing(m_State) && type == eGL_DEBUG_TYPE_MARKER)
  {
    USE_SCRATCH_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDebugMessageInsert(ser, source, type, id, severity, length, buf);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glPushGroupMarkerEXT(GLsizei length, const GLchar *marker)
{
  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPushDebugGroup(ser, eGL_DEBUG_SOURCE_APPLICATION, 0, length, marker);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glPopGroupMarkerEXT()
{
  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPopDebugGroup(ser);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glInsertEventMarkerEXT(SerialiserType &ser, GLsizei length,
                                                     const GLchar *marker_)
{
  std::string marker =
      marker_ ? std::string(marker_, marker_ + (length > 0 ? length : strlen(marker_))) : "";

  SERIALISE_ELEMENT(length);
  SERIALISE_ELEMENT(marker);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(m_ReplayMarkers)
      GLMarkerRegion::Set(marker);

    if(IsLoading(m_State))
    {
      DrawcallDescription draw;
      draw.name = marker;
      draw.flags |= DrawFlags::SetMarker;

      AddEvent();
      AddDrawcall(draw, false);
    }
  }

  return true;
}

void WrappedOpenGL::glInsertEventMarkerEXT(GLsizei length, const GLchar *marker)
{
  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glInsertEventMarkerEXT(ser, length, marker);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glFrameTerminatorGREMEDY()
{
  SwapBuffers((void *)m_ActiveContexts[Threading::GetCurrentID()].wnd);
}

void WrappedOpenGL::glStringMarkerGREMEDY(GLsizei len, const void *string)
{
  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glInsertEventMarkerEXT(ser, len, (const GLchar *)string);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPushDebugGroup(SerialiserType &ser, GLenum source, GLuint id,
                                               GLsizei length, const GLchar *message_)
{
  std::string message =
      message_ ? std::string(message_, message_ + (length > 0 ? length : strlen(message_))) : "";

  // unused, just for the user's benefit
  SERIALISE_ELEMENT(source);
  SERIALISE_ELEMENT(id);
  SERIALISE_ELEMENT(length);
  SERIALISE_ELEMENT(message);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(m_ReplayMarkers)
    {
      GLMarkerRegion::Begin(message, source, id);
      m_ReplayEventCount++;
    }

    if(IsLoading(m_State))
    {
      DrawcallDescription draw;
      draw.name = message;
      draw.flags |= DrawFlags::PushMarker;

      AddEvent();
      AddDrawcall(draw, false);
    }
  }

  return true;
}

void WrappedOpenGL::glPushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar *message)
{
  if(GL.glPushDebugGroup)
  {
    SERIALISE_TIME_CALL(GL.glPushDebugGroup(source, id, length, message));
  }
  else
  {
    m_ScratchSerialiser.ChunkMetadata().timestampMicro = RenderDoc::Inst().GetMicrosecondTimestamp();
    m_ScratchSerialiser.ChunkMetadata().durationMicro = 0;
  }

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPushDebugGroup(ser, source, id, length, message);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPopDebugGroup(SerialiserType &ser)
{
  if(IsReplayingAndReading())
  {
    if(m_ReplayMarkers)
      GLMarkerRegion::End();
    m_ReplayEventCount = RDCMAX(0, m_ReplayEventCount - 1);

    if(IsLoading(m_State) && HasNonDebugMarkers())
    {
      DrawcallDescription draw;
      draw.name = "API Calls";
      draw.flags |= DrawFlags::APICalls;

      AddDrawcall(draw, true);
    }
  }

  return true;
}
void WrappedOpenGL::glPopDebugGroup()
{
  if(GL.glPopDebugGroup)
  {
    SERIALISE_TIME_CALL(GL.glPopDebugGroup());
  }
  else
  {
    m_ScratchSerialiser.ChunkMetadata().timestampMicro = RenderDoc::Inst().GetMicrosecondTimestamp();
    m_ScratchSerialiser.ChunkMetadata().durationMicro = 0;
  }

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPopDebugGroup(ser);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

// these get functions are here instead of gl_get_funcs.cpp because we have a local implementation
// for in case the driver doesn't support them
void WrappedOpenGL::glGetObjectLabelEXT(GLenum type, GLuint object, GLsizei bufSize,
                                        GLsizei *length, GLchar *label)
{
  if(GL.glGetObjectLabelEXT)
  {
    GL.glGetObjectLabelEXT(type, object, bufSize, length, label);
  }
  else
  {
    GLResource res = GetResource(type, object);

    ReturnObjectlabel(GetResourceManager()->GetName(res), bufSize, length, label);
  }
}

GLuint WrappedOpenGL::glGetDebugMessageLog(GLuint count, GLsizei bufSize, GLenum *sources,
                                           GLenum *types, GLuint *ids, GLenum *severities,
                                           GLsizei *lengths, GLchar *messageLog)
{
  if(GL.glGetDebugMessageLog)
  {
    return GL.glGetDebugMessageLog(count, bufSize, sources, types, ids, severities, lengths,
                                   messageLog);
  }
  else
  {
    return 0;
  }
}

void WrappedOpenGL::glGetObjectLabel(GLenum identifier, GLuint name, GLsizei bufSize,
                                     GLsizei *length, GLchar *label)
{
  if(GL.glGetObjectLabel)
  {
    GL.glGetObjectLabel(identifier, name, bufSize, length, label);
  }
  else
  {
    GLResource res = GetResource(identifier, name);

    ReturnObjectlabel(GetResourceManager()->GetName(res), bufSize, length, label);
  }
}

void WrappedOpenGL::glGetObjectPtrLabel(const void *ptr, GLsizei bufSize, GLsizei *length,
                                        GLchar *label)
{
  if(GL.glGetObjectPtrLabel)
  {
    GL.glGetObjectPtrLabel(ptr, bufSize, length, label);
  }
  else
  {
    ResourceId id = GetResourceManager()->GetSyncID((GLsync)ptr);

    ReturnObjectlabel(GetResourceManager()->GetName(id), bufSize, length, label);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(void, glObjectLabel, GLenum identifier, GLuint name, GLsizei length,
                                const GLchar *label);
INSTANTIATE_FUNCTION_SERIALISED(void, glDebugMessageInsert, GLenum source, GLenum type, GLuint id,
                                GLenum severity, GLsizei length, const GLchar *buf);
INSTANTIATE_FUNCTION_SERIALISED(void, glInsertEventMarkerEXT, GLsizei length, const GLchar *marker);
INSTANTIATE_FUNCTION_SERIALISED(void, glPushDebugGroup, GLenum source, GLuint id, GLsizei length,
                                const GLchar *message);
INSTANTIATE_FUNCTION_SERIALISED(void, glPopDebugGroup);
