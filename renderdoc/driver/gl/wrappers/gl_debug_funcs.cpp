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
#include "strings/string_utils.h"

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glObjectLabel(SerialiserType &ser, GLenum identifier, GLuint name,
                                            GLsizei length, const GLchar *label)
{
  GLResource Resource;
  std::string Label;

  if(ser.IsWriting())
  {
    if(length == 0 || label == NULL)
      Label = "";
    else
      Label = std::string(label, label + (length > 0 ? length : strlen(label)));

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
  }

  SERIALISE_ELEMENT(Resource);
  SERIALISE_ELEMENT(length);
  SERIALISE_ELEMENT(Label);

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
  m_Real.glLabelObjectEXT(identifier, name, length, label);

  if(IsCaptureMode(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glObjectLabel(ser, identifier, name, length, label);

    m_DeviceRecord->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glObjectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar *label)
{
  m_Real.glObjectLabel(identifier, name, length, label);

  if(IsCaptureMode(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glObjectLabel(ser, identifier, name, length, label);

    m_DeviceRecord->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glObjectPtrLabel(const void *ptr, GLsizei length, const GLchar *label)
{
  m_Real.glObjectPtrLabel(ptr, length, label);

  if(IsCaptureMode(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    ResourceId id = GetResourceManager()->GetSyncID((GLsync)ptr);
    Serialise_glObjectLabel(ser, eGL_SYNC_FENCE, GetResourceManager()->GetCurrentResource(id).name,
                            length, label);

    m_DeviceRecord->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glDebugMessageCallback(GLDEBUGPROC callback, const void *userParam)
{
  m_RealDebugFunc = callback;
  m_RealDebugFuncParam = userParam;

  m_Real.glDebugMessageCallback(&DebugSnoopStatic, this);
}

void WrappedOpenGL::glDebugMessageControl(GLenum source, GLenum type, GLenum severity,
                                          GLsizei count, const GLuint *ids, GLboolean enabled)
{
  // we could exert control over debug messages here
  m_Real.glDebugMessageControl(source, type, severity, count, ids, enabled);
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
  SERIALISE_ELEMENT(name);

  if(IsReplayingAndReading())
  {
    GLMarkerRegion::Set(name);

    if(IsLoading(m_State))
    {
      DrawcallDescription draw;
      draw.name = name;
      draw.flags |= DrawFlags::SetMarker;

      AddDrawcall(draw, false);
    }
  }

  return true;
}

void WrappedOpenGL::glDebugMessageInsert(GLenum source, GLenum type, GLuint id, GLenum severity,
                                         GLsizei length, const GLchar *buf)
{
  if(IsActiveCapturing(m_State) && type == eGL_DEBUG_TYPE_MARKER)
  {
    USE_SCRATCH_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDebugMessageInsert(ser, source, type, id, severity, length, buf);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_Real.glDebugMessageInsert(source, type, id, severity, length, buf);
}

void WrappedOpenGL::glPushGroupMarkerEXT(GLsizei length, const GLchar *marker)
{
  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPushDebugGroup(ser, eGL_DEBUG_SOURCE_APPLICATION, 0, length, marker);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glPopGroupMarkerEXT()
{
  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPopDebugGroup(ser);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glInsertEventMarkerEXT(SerialiserType &ser, GLsizei length,
                                                     const GLchar *marker_)
{
  std::string marker =
      marker_ ? std::string(marker_, marker_ + (length > 0 ? length : strlen(marker_))) : "";

  SERIALISE_ELEMENT(marker);

  if(IsReplayingAndReading())
  {
    GLMarkerRegion::Set(marker);

    if(IsLoading(m_State))
    {
      DrawcallDescription draw;
      draw.name = marker;
      draw.flags |= DrawFlags::SetMarker;

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

    m_ContextRecord->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glFrameTerminatorGREMEDY()
{
  SwapBuffers(NULL);
}

void WrappedOpenGL::glStringMarkerGREMEDY(GLsizei len, const void *string)
{
  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glInsertEventMarkerEXT(ser, len, (const GLchar *)string);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPushDebugGroup(SerialiserType &ser, GLenum source, GLuint id,
                                               GLsizei length, const GLchar *message)
{
  std::string name =
      message ? std::string(message, message + (length > 0 ? length : strlen(message))) : "";

  // unused, just for the user's benefit
  SERIALISE_ELEMENT(source);
  SERIALISE_ELEMENT(id);
  SERIALISE_ELEMENT(name);

  if(IsReplayingAndReading())
  {
    GLMarkerRegion::Begin(name, source, id);
    m_ReplayEventCount++;

    if(IsLoading(m_State))
    {
      DrawcallDescription draw;
      draw.name = name;
      draw.flags |= DrawFlags::PushMarker;

      AddDrawcall(draw, false);
    }
  }

  return true;
}

void WrappedOpenGL::glPushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar *message)
{
  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPushDebugGroup(ser, source, id, length, message);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_Real.glPushDebugGroup(source, id, length, message);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPopDebugGroup(SerialiserType &ser)
{
  if(IsReplayingAndReading())
  {
    GLMarkerRegion::End();
    m_ReplayEventCount = RDCMAX(0, m_ReplayEventCount - 1);

    if(IsLoading(m_State) && !m_CurEvents.empty())
    {
      DrawcallDescription draw;
      draw.name = "API Calls";
      draw.flags |= DrawFlags::SetMarker | DrawFlags::APICalls;

      AddDrawcall(draw, true);
    }
  }

  return true;
}
void WrappedOpenGL::glPopDebugGroup()
{
  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPopDebugGroup(ser);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_Real.glPopDebugGroup();
}

INSTANTIATE_FUNCTION_SERIALISED(void, glObjectLabel, GLenum identifier, GLuint name, GLsizei length,
                                const GLchar *label);
INSTANTIATE_FUNCTION_SERIALISED(void, glDebugMessageInsert, GLenum source, GLenum type, GLuint id,
                                GLenum severity, GLsizei length, const GLchar *buf);
INSTANTIATE_FUNCTION_SERIALISED(void, glInsertEventMarkerEXT, GLsizei length, const GLchar *marker);
INSTANTIATE_FUNCTION_SERIALISED(void, glPushDebugGroup, GLenum source, GLuint id, GLsizei length,
                                const GLchar *message);
INSTANTIATE_FUNCTION_SERIALISED(void, glPopDebugGroup);
