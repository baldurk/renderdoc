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

bool WrappedOpenGL::Serialise_glObjectLabel(GLenum identifier, GLuint name, GLsizei length,
                                            const GLchar *label)
{
  ResourceId liveid;

  bool extvariant = false;

  string Label;
  if(m_State >= WRITING)
  {
    if(length == 0)
      Label = "";
    else
      Label = string(label, label + (length > 0 ? length : strlen(label)));

    switch(identifier)
    {
      case eGL_TEXTURE: liveid = GetResourceManager()->GetID(TextureRes(GetCtx(), name)); break;
      case eGL_BUFFER_OBJECT_EXT:
        extvariant = true;    // intentional fallthrough
      case eGL_BUFFER: liveid = GetResourceManager()->GetID(BufferRes(GetCtx(), name)); break;
      case eGL_PROGRAM_OBJECT_EXT:
        extvariant = true;    // intentional fallthrough
      case eGL_PROGRAM: liveid = GetResourceManager()->GetID(ProgramRes(GetCtx(), name)); break;
      case eGL_PROGRAM_PIPELINE_OBJECT_EXT:
        extvariant = true;    // intentional fallthrough
      case eGL_PROGRAM_PIPELINE:
        liveid = GetResourceManager()->GetID(ProgramPipeRes(GetCtx(), name));
        break;
      case eGL_VERTEX_ARRAY_OBJECT_EXT:
        extvariant = true;    // intentional fallthrough
      case eGL_VERTEX_ARRAY:
        liveid = GetResourceManager()->GetID(VertexArrayRes(GetCtx(), name));
        break;
      case eGL_SHADER_OBJECT_EXT:
        extvariant = true;    // intentional fallthrough
      case eGL_SHADER: liveid = GetResourceManager()->GetID(ShaderRes(GetCtx(), name)); break;
      case eGL_QUERY_OBJECT_EXT:
        extvariant = true;    // intentional fallthrough
      case eGL_QUERY: liveid = GetResourceManager()->GetID(QueryRes(GetCtx(), name)); break;
      case eGL_TRANSFORM_FEEDBACK:
        liveid = GetResourceManager()->GetID(FeedbackRes(GetCtx(), name));
        break;
      case eGL_SAMPLER: liveid = GetResourceManager()->GetID(SamplerRes(GetCtx(), name)); break;
      case eGL_RENDERBUFFER:
        liveid = GetResourceManager()->GetID(RenderbufferRes(GetCtx(), name));
        break;
      case eGL_FRAMEBUFFER:
        liveid = GetResourceManager()->GetID(FramebufferRes(GetCtx(), name));
        break;
      default: RDCERR("Unhandled namespace in glObjectLabel");
    }
  }

  SERIALISE_ELEMENT(GLenum, Identifier, identifier);
  SERIALISE_ELEMENT(ResourceId, id, liveid);
  SERIALISE_ELEMENT(uint32_t, Length, length);
  SERIALISE_ELEMENT(bool, HasLabel, label != NULL);

  m_pSerialiser->SerialiseString("label", Label);

  if(m_State == READING && GetResourceManager()->HasLiveResource(id))
    GetResourceManager()->SetName(id, HasLabel ? Label : "");

  return true;
}

void WrappedOpenGL::glLabelObjectEXT(GLenum identifier, GLuint name, GLsizei length,
                                     const GLchar *label)
{
  m_Real.glLabelObjectEXT(identifier, name, length, label);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(OBJECT_LABEL);
    Serialise_glObjectLabel(identifier, name, length, label);

    m_DeviceRecord->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glObjectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar *label)
{
  m_Real.glObjectLabel(identifier, name, length, label);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(OBJECT_LABEL);
    Serialise_glObjectLabel(identifier, name, length, label);

    m_DeviceRecord->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glObjectPtrLabel(const void *ptr, GLsizei length, const GLchar *label)
{
  m_Real.glObjectPtrLabel(ptr, length, label);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(OBJECT_LABEL);
    ResourceId id = GetResourceManager()->GetSyncID((GLsync)ptr);
    Serialise_glObjectLabel(eGL_SYNC_FENCE, GetResourceManager()->GetCurrentResource(id).name,
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

bool WrappedOpenGL::Serialise_glDebugMessageInsert(GLenum source, GLenum type, GLuint id,
                                                   GLenum severity, GLsizei length, const GLchar *buf)
{
  string name = buf ? string(buf, buf + (length > 0 ? length : strlen(buf))) : "";

  m_pSerialiser->Serialise("Name", name);

  if(m_State == READING)
  {
    DrawcallDescription draw;
    draw.name = name;
    draw.flags |= DrawFlags::SetMarker;

    AddDrawcall(draw, false);
  }

  return true;
}

void WrappedOpenGL::glDebugMessageInsert(GLenum source, GLenum type, GLuint id, GLenum severity,
                                         GLsizei length, const GLchar *buf)
{
  if(m_State == WRITING_CAPFRAME && type == eGL_DEBUG_TYPE_MARKER)
  {
    SCOPED_SERIALISE_CONTEXT(SET_MARKER);
    Serialise_glDebugMessageInsert(source, type, id, severity, length, buf);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_Real.glDebugMessageInsert(source, type, id, severity, length, buf);
}

void WrappedOpenGL::glPushGroupMarkerEXT(GLsizei length, const GLchar *marker)
{
  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(BEGIN_EVENT);
    Serialise_glPushDebugGroup(eGL_NONE, 0, length, marker);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glPopGroupMarkerEXT()
{
  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(END_EVENT);
    Serialise_glPopDebugGroup();

    m_ContextRecord->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glInsertEventMarkerEXT(GLsizei length, const GLchar *marker)
{
  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(SET_MARKER);
    Serialise_glDebugMessageInsert(eGL_NONE, eGL_NONE, 0, eGL_NONE, length, marker);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glFrameTerminatorGREMEDY()
{
  SwapBuffers(NULL);
}

void WrappedOpenGL::glStringMarkerGREMEDY(GLsizei len, const void *string)
{
  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(SET_MARKER);
    Serialise_glDebugMessageInsert(eGL_NONE, eGL_NONE, 0, eGL_NONE, len, (const GLchar *)string);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glPushDebugGroup(GLenum source, GLuint id, GLsizei length,
                                               const GLchar *message)
{
  string name = message ? string(message, message + (length > 0 ? length : strlen(message))) : "";

  m_pSerialiser->Serialise("Name", name);

  if(m_State == READING)
  {
    DrawcallDescription draw;
    draw.name = name;
    draw.flags |= DrawFlags::PushMarker;

    AddDrawcall(draw, false);
  }

  return true;
}

void WrappedOpenGL::glPushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar *message)
{
  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(BEGIN_EVENT);
    Serialise_glPushDebugGroup(source, id, length, message);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_Real.glPushDebugGroup(source, id, length, message);
}

bool WrappedOpenGL::Serialise_glPopDebugGroup()
{
  if(m_State == READING && !m_CurEvents.empty())
  {
    DrawcallDescription draw;
    draw.name = "API Calls";
    draw.flags |= DrawFlags::SetMarker | DrawFlags::APICalls;

    AddDrawcall(draw, true);
  }

  return true;
}
void WrappedOpenGL::glPopDebugGroup()
{
  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(END_EVENT);
    Serialise_glPopDebugGroup();

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_Real.glPopDebugGroup();
}
