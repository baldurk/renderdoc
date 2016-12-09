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

#include "driver/gles/gles_manager.h"
#include "driver/gles/gles_driver.h"

struct VertexAttribInitialData
{
  uint32_t enabled;
  uint32_t vbslot;
  uint32_t offset;
  GLenum type;
  int32_t normalized;
  uint32_t integer;
  uint32_t size;
};

struct VertexBufferInitialData
{
  ResourceId Buffer;
  uint64_t Stride;
  uint64_t Offset;
  uint32_t Divisor;
};

// note these data structures below contain a 'valid' bool, since due to complexities of
// fetching the state on the right context, we might never be able to fetch the data at
// all. So the valid is set to false to indicate that we shouldn't try to restore it on
// replay.
struct VAOInitialData
{
  bool valid;
  VertexAttribInitialData VertexAttribs[16];
  VertexBufferInitialData VertexBuffers[16];
  ResourceId ElementArrayBuffer;
};

struct FeedbackInitialData
{
  bool valid;
  ResourceId Buffer[4];
  uint64_t Offset[4];
  uint64_t Size[4];
};

struct FramebufferAttachmentData
{
  bool renderbuffer;
  bool layered;
  int32_t layer;
  int32_t level;
  ResourceId obj;
};

struct FramebufferInitialData
{
  bool valid;
  FramebufferAttachmentData Attachments[10];
  GLenum DrawBuffers[8];
  GLenum ReadBuffer;

  static const GLenum attachmentNames[10];
};

const GLenum FramebufferInitialData::attachmentNames[10] = {
    eGL_COLOR_ATTACHMENT0, eGL_COLOR_ATTACHMENT1,  eGL_COLOR_ATTACHMENT2, eGL_COLOR_ATTACHMENT3,
    eGL_COLOR_ATTACHMENT4, eGL_COLOR_ATTACHMENT5,  eGL_COLOR_ATTACHMENT6, eGL_COLOR_ATTACHMENT7,
    eGL_DEPTH_ATTACHMENT,  eGL_STENCIL_ATTACHMENT,
};

template <>
void Serialiser::Serialise(const char *name, VertexAttribInitialData &el)
{
  ScopedContext scope(this, name, "VertexArrayInitialData", 0, true);
  Serialise("enabled", el.enabled);
  Serialise("vbslot", el.vbslot);
  Serialise("offset", el.offset);
  Serialise("type", el.type);
  Serialise("normalized", el.normalized);
  Serialise("integer", el.integer);
  Serialise("size", el.size);
}

template <>
void Serialiser::Serialise(const char *name, VertexBufferInitialData &el)
{
  ScopedContext scope(this, name, "VertexBufferInitialData", 0, true);
  Serialise("Buffer", el.Buffer);
  Serialise("Stride", el.Stride);
  Serialise("Offset", el.Offset);
  Serialise("Divisor", el.Divisor);
}

template <>
void Serialiser::Serialise(const char *name, FeedbackInitialData &el)
{
  ScopedContext scope(this, name, "FeedbackInitialData", 0, true);
  Serialise("valid", el.valid);
  SerialisePODArray<4>("Buffer", el.Buffer);
  SerialisePODArray<4>("Offset", el.Offset);
  SerialisePODArray<4>("Size", el.Size);
}

template <>
void Serialiser::Serialise(const char *name, FramebufferAttachmentData &el)
{
  ScopedContext scope(this, name, "FramebufferAttachmentData", 0, true);
  Serialise("renderbuffer", el.renderbuffer);
  Serialise("layered", el.layered);
  Serialise("layer", el.layer);
  Serialise("level", el.level);
  Serialise("obj", el.obj);
}

template <>
void Serialiser::Serialise(const char *name, FramebufferInitialData &el)
{
  ScopedContext scope(this, name, "FramebufferInitialData", 0, true);
  Serialise("valid", el.valid);
  SerialisePODArray<8>("DrawBuffers", el.DrawBuffers);
  for(size_t i = 0; i < ARRAY_COUNT(el.Attachments); i++)
    Serialise("Attachments", el.Attachments[i]);
  Serialise("ReadBuffer", el.ReadBuffer);
}

struct TextureStateInitialData
{
  int32_t baseLevel, maxLevel;
  float minLod, maxLod;
  GLenum srgbDecode;
  GLenum depthMode;
  GLenum compareFunc, compareMode;
  GLenum minFilter, magFilter;
  int32_t seamless;
  GLenum swizzle[4];
  GLenum wrap[3];
  float border[4];
  float lodBias;
  ResourceId texBuffer;
  uint32_t texBufOffs;
  uint32_t texBufSize;
};

template <>
void Serialiser::Serialise(const char *name, TextureStateInitialData &el)
{
  ScopedContext scope(this, name, "TextureStateInitialData", 0, true);
  Serialise("baseLevel", el.baseLevel);
  Serialise("maxLevel", el.maxLevel);
  Serialise("minLod", el.minLod);
  Serialise("maxLod", el.maxLod);
  Serialise("srgbDecode", el.srgbDecode);
  Serialise("depthMode", el.depthMode);
  Serialise("compareFunc", el.compareFunc);
  Serialise("compareMode", el.compareMode);
  Serialise("seamless", el.seamless);
  Serialise("minFilter", el.minFilter);
  Serialise("magFilter", el.magFilter);
  SerialisePODArray<4>("swizzle", el.swizzle);
  SerialisePODArray<3>("wrap", el.wrap);
  SerialisePODArray<4>("border", el.border);
  Serialise("lodBias", el.lodBias);
  Serialise("texBuffer", el.texBuffer);
  Serialise("texBufOffs", el.texBufOffs);
  Serialise("texBufSize", el.texBufSize);
}

void GLResourceManager::MarkVAOReferenced(GLResource res, FrameRefType ref, bool allowFake0)
{
  const GLHookSet &gl = m_GL->m_Real;

  if(res.name || allowFake0)
  {
    ResourceManager::MarkResourceFrameReferenced(GetID(res), ref == eFrameRef_Unknown ? eFrameRef_Unknown : eFrameRef_Read);

    GLint numVBufferBindings = 16;
    gl.glGetIntegerv(eGL_MAX_VERTEX_ATTRIB_BINDINGS, &numVBufferBindings);

    for(GLuint i = 0; i < (GLuint)numVBufferBindings; i++)
    {
      GLuint buffer = GetBoundVertexBuffer(gl, i);

      MarkResourceFrameReferenced(BufferRes(res.Context, buffer), ref);
    }

    GLuint ibuffer = 0;
    gl.glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, (GLint *)&ibuffer);
    MarkResourceFrameReferenced(BufferRes(res.Context, ibuffer), ref);
  }
}

void GLResourceManager::MarkFBOReferenced(GLResource res, FrameRefType ref)
{
  if(res.name == 0)
    return;

  MarkResourceFrameReferenced(res, ref == eFrameRef_Unknown ? eFrameRef_Unknown : eFrameRef_Read);

  const GLHookSet &gl = m_GL->m_Real;

  GLint numCols = 8;
  gl.glGetIntegerv(eGL_MAX_COLOR_ATTACHMENTS, &numCols);

  GLenum type = eGL_TEXTURE;
  GLuint name = 0;

  GLuint oldBinding;
  gl.glGetIntegerv(eGL_FRAMEBUFFER_BINDING, (GLint*)&oldBinding);
  gl.glBindFramebuffer(eGL_FRAMEBUFFER, res.name);
  for(int c = 0; c < numCols; c++)
  {
    gl.glGetFramebufferAttachmentParameteriv(eGL_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + c),
                                                     eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                     (GLint *)&name);
    gl.glGetFramebufferAttachmentParameteriv(eGL_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + c),
                                                     eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                     (GLint *)&type);

    if(type == eGL_RENDERBUFFER)
      MarkResourceFrameReferenced(RenderbufferRes(res.Context, name), ref);
    else
      MarkResourceFrameReferenced(TextureRes(res.Context, name), ref);
  }

  gl.glGetFramebufferAttachmentParameteriv(
      eGL_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&name);
  gl.glGetFramebufferAttachmentParameteriv(
      eGL_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

  if(name)
  {
    if(type == eGL_RENDERBUFFER)
      MarkResourceFrameReferenced(RenderbufferRes(res.Context, name), ref);
    else
      MarkResourceFrameReferenced(TextureRes(res.Context, name), ref);
  }

  gl.glGetFramebufferAttachmentParameteriv(
      eGL_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&name);
  gl.glGetFramebufferAttachmentParameteriv(
      eGL_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

  if(name)
  {
    if(type == eGL_RENDERBUFFER)
      MarkResourceFrameReferenced(RenderbufferRes(res.Context, name), ref);
    else
      MarkResourceFrameReferenced(TextureRes(res.Context, name), ref);
  }

  gl.glBindFramebuffer(eGL_FRAMEBUFFER, oldBinding);
}

bool GLResourceManager::SerialisableResource(ResourceId id, GLResourceRecord *record)
{
  if(id == m_GL->GetContextResourceID())
    return false;
  return true;
}

bool GLResourceManager::Need_InitialStateChunk(GLResource res)
{
  return res.Namespace != eResBuffer;
}

bool GLResourceManager::Prepare_InitialState(GLResource res, byte *blob)
{
  const GLHookSet &gl = m_GL->m_Real;

  if(res.Namespace == eResFramebuffer)
  {
    FramebufferInitialData *data = (FramebufferInitialData *)blob;

    data->valid = true;

    GLuint prevread = 0, prevdraw = 0;
    gl.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&prevdraw);
    gl.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&prevread);

    gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, res.name);
    gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, res.name);

    // need to serialise out which objects are bound
    GLenum type = eGL_TEXTURE;
    GLuint object = 0;
    GLint layered = 0;
    for(int i = 0; i < (int)ARRAY_COUNT(data->Attachments); i++)
    {
      FramebufferAttachmentData &a = data->Attachments[i];

      gl.glGetFramebufferAttachmentParameteriv(eGL_FRAMEBUFFER, data->attachmentNames[i],
                                                       eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                       (GLint *)&object);
      gl.glGetFramebufferAttachmentParameteriv(
          eGL_FRAMEBUFFER, data->attachmentNames[i], eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

      if(object)
      {
        a.level = 0;
        gl.glGetFramebufferAttachmentParameteriv(
            eGL_FRAMEBUFFER, data->attachmentNames[i], eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &a.level);
        gl.glGetFramebufferAttachmentParameteriv(
            eGL_FRAMEBUFFER, data->attachmentNames[i], eGL_FRAMEBUFFER_ATTACHMENT_LAYERED, &layered);

        a.layer = 0;
        if(layered == 0)
          gl.glGetFramebufferAttachmentParameteriv(
              eGL_FRAMEBUFFER, data->attachmentNames[i], eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, &a.layer);
      }

      a.layered = (layered != 0);
      a.renderbuffer = (type == eGL_RENDERBUFFER);
      a.obj = GetID(a.renderbuffer ? RenderbufferRes(res.Context, object)
                                   : TextureRes(res.Context, object));

      if(!a.renderbuffer)
      {
        WrappedGLES::TextureData &details = m_GL->m_Textures[a.obj];

        if(details.curType == eGL_TEXTURE_CUBE_MAP)
        {
          GLenum face;
          gl.glGetFramebufferAttachmentParameteriv(
              eGL_FRAMEBUFFER, data->attachmentNames[i], eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE,
              (GLint *)&face);

          a.layer = CubeTargetIndex(face);
        }
      }
    }

    for(int i = 0; i < (int)ARRAY_COUNT(data->DrawBuffers); i++)
      gl.glGetIntegerv(GLenum(eGL_DRAW_BUFFER0 + i), (GLint *)&data->DrawBuffers[i]);

    gl.glGetIntegerv(eGL_READ_BUFFER, (GLint *)&data->ReadBuffer);

    gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, prevdraw);
    gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, prevread);
  }
  else if(res.Namespace == eResFeedback)
  {
    FeedbackInitialData *data = (FeedbackInitialData *)blob;

    data->valid = true;

    GLuint prevfeedback = 0;
    gl.glGetIntegerv(eGL_TRANSFORM_FEEDBACK_BINDING, (GLint *)&prevfeedback);

    gl.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, res.name);

    GLint maxCount = 0;
    gl.glGetIntegerv(eGL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &maxCount);

    for(int i = 0; i < (int)ARRAY_COUNT(data->Buffer) && i < maxCount; i++)
    {
      GLuint buffer = 0;
      gl.glGetIntegeri_v(eGL_TRANSFORM_FEEDBACK_BUFFER_BINDING, i, (GLint *)&buffer);
      data->Buffer[i] = GetID(BufferRes(res.Context, buffer));
      gl.glGetInteger64i_v(eGL_TRANSFORM_FEEDBACK_BUFFER_START, i, (GLint64 *)&data->Offset[i]);
      gl.glGetInteger64i_v(eGL_TRANSFORM_FEEDBACK_BUFFER_SIZE, i, (GLint64 *)&data->Size[i]);
    }

    gl.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, prevfeedback);
  }
  else if(res.Namespace == eResVertexArray)
  {
    VAOInitialData *data = (VAOInitialData *)blob;

    data->valid = true;

    GLuint prevVAO = 0;
    gl.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&prevVAO);

    if(res.name == 0)
      gl.glBindVertexArray(m_GL->GetFakeVAO());
    else
      gl.glBindVertexArray(res.name);

    for(GLuint i = 0; i < 16; i++)
    {
      gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_ENABLED,
                             (GLint *)&data->VertexAttribs[i].enabled);
      gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_BINDING, (GLint *)&data->VertexAttribs[i].vbslot);
      gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_RELATIVE_OFFSET,
                             (GLint *)&data->VertexAttribs[i].offset);
      gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_TYPE, (GLint *)&data->VertexAttribs[i].type);
      gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_NORMALIZED,
                             (GLint *)&data->VertexAttribs[i].normalized);
      gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_INTEGER,
                             (GLint *)&data->VertexAttribs[i].integer);
      gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_SIZE, (GLint *)&data->VertexAttribs[i].size);

      GLuint buffer = GetBoundVertexBuffer(gl, i);

      data->VertexBuffers[i].Buffer = GetID(BufferRes(res.Context, buffer));

      gl.glGetIntegeri_v(eGL_VERTEX_BINDING_STRIDE, i, (GLint *)&data->VertexBuffers[i].Stride);
      gl.glGetIntegeri_v(eGL_VERTEX_BINDING_OFFSET, i, (GLint *)&data->VertexBuffers[i].Offset);
      gl.glGetIntegeri_v(eGL_VERTEX_BINDING_DIVISOR, i, (GLint *)&data->VertexBuffers[i].Divisor);
    }

    GLuint buffer = 0;
    gl.glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, (GLint *)&buffer);
    data->ElementArrayBuffer = GetID(BufferRes(res.Context, buffer));

    gl.glBindVertexArray(prevVAO);
  }

  return true;
}

bool GLResourceManager::Prepare_InitialState(GLResource res)
{
  // this function needs to be refactored to better deal with multiple
  // contexts and resources that are specific to a particular context

  ResourceId Id = GetID(res);

  const GLHookSet &gl = m_GL->m_Real;

  if(res.Namespace == eResBuffer)
  {
    GLResourceRecord *record = GetResourceRecord(res);

    // TODO copy this to an immutable buffer elsewhere and SetInitialContents() it.
    // then only do the readback in Serialise_InitialState
    GLint length;
    SafeBufferBinder safeBufferBinder(m_GL->m_Real, record->datatype, res.name);
    gl.glGetBufferParameteriv(record->datatype, eGL_BUFFER_SIZE, &length);
    m_GL->Compat_glGetBufferSubData(record->datatype, 0, length, record->GetDataPtr());
  }
  else if(res.Namespace == eResProgram)
  {
    ScopedContext scope(m_pSerialiser, "Initial Contents", "Initial Contents", INITIAL_CONTENTS,
                        false);

    m_pSerialiser->Serialise("Id", Id);

    SerialiseProgramUniforms(gl, m_pSerialiser, res.name, NULL, true);

    SetInitialChunk(Id, scope.Get());
  }
  else if(res.Namespace == eResTexture)
  {
    PrepareTextureInitialContents(Id, Id, res);
  }
  else if(res.Namespace == eResFramebuffer)
  {
    byte *data = Serialiser::AllocAlignedBuffer(sizeof(FramebufferInitialData));
    RDCEraseMem(data, sizeof(FramebufferInitialData));

    SetInitialContents(Id, InitialContentData(GLResource(MakeNullResource), 0, data));

    // if FBOs aren't shared we need to fetch the data for this FBO on the right context. It's
    // not safe for us to go changing contexts ourselves (the context could be active on another
    // thread), so instead we'll queue this up to fetch when we are on the correct context.
    //
    // Because we've already allocated and set the blob above, it can be filled in any time
    // before serialising (end of the frame, and if the context is never used before the end of
    // the frame the resource can't be used, so not fetching the initial state doesn't matter).
    //
    // Note we also need to detect the case where the context is already current on another thread
    // and we just start getting commands there, but that case already isn't supported as we don't
    // detect it and insert state-change chunks, we assume all commands will come from a single
    // thread.
    if(!VendorCheck[VendorCheck_EXT_fbo_shared] && res.Context && m_GL->GetCtx() != res.Context)
    {
      m_GL->QueuePrepareInitialState(res, data);
    }
    else
    {
      // call immediately, we are on the right context or for one reason or another the context
      // doesn't matter for fetching this resource (res.Context is NULL or vendorcheck means they're
      // shared).
      Prepare_InitialState(res, (byte *)data);
    }
  }
  else if(res.Namespace == eResFeedback)
  {
    byte *data = Serialiser::AllocAlignedBuffer(sizeof(FeedbackInitialData));
    RDCEraseMem(data, sizeof(FeedbackInitialData));

    SetInitialContents(Id, InitialContentData(GLResource(MakeNullResource), 0, data));

    // queue initial state fetching if we're not on the right context, see above in FBOs for more
    // explanation of this.
    if(res.Context && m_GL->GetCtx() != res.Context)
    {
      m_GL->QueuePrepareInitialState(res, data);
    }
    else
    {
      Prepare_InitialState(res, (byte *)data);
    }
  }
  else if(res.Namespace == eResVertexArray)
  {
    byte *data = Serialiser::AllocAlignedBuffer(sizeof(VAOInitialData));
    RDCEraseMem(data, sizeof(VAOInitialData));

    SetInitialContents(Id, InitialContentData(GLResource(MakeNullResource), 0, data));

    // queue initial state fetching if we're not on the right context, see above in FBOs for more
    // explanation of this.
    if(res.Context && m_GL->GetCtx() != res.Context)
    {
      m_GL->QueuePrepareInitialState(res, data);
    }
    else
    {
      Prepare_InitialState(res, (byte *)data);
    }
  }
  else if(res.Namespace == eResRenderbuffer)
  {
    //
  }
  else
  {
    RDCERR("Unexpected type of resource requiring initial state");
  }

  return true;
}

void GLResourceManager::PrepareTextureInitialContents(ResourceId liveid, ResourceId origid,
                                                      GLResource res)
{
  const GLHookSet &gl = m_GL->m_Real;

  WrappedGLES::TextureData &details = m_GL->m_Textures[liveid];

  TextureStateInitialData *state =
      (TextureStateInitialData *)Serialiser::AllocAlignedBuffer(sizeof(TextureStateInitialData));
  RDCEraseMem(state, sizeof(TextureStateInitialData));

  if(details.internalFormat == eGL_NONE)
  {
    // textures can get here as GL_NONE if they were created and dirtied (by setting lots of
    // texture parameters) without ever having storage allocated (via glTexStorage or glTexImage).
    // in that case, just ignore as we won't bother with the initial states.
    SetInitialContents(origid, InitialContentData(GLResource(MakeNullResource), 0, (byte *)state));
  }
  else if(details.curType != eGL_TEXTURE_BUFFER)
  {
    GLuint oldtex = 0;
    GLenum binding = TextureBinding(details.curType);
    gl.glGetIntegerv(binding, (GLint *)&oldtex);
    gl.glBindTexture(details.curType, res.name);

    bool ms = (details.curType == eGL_TEXTURE_2D_MULTISAMPLE ||
               details.curType == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY);

    state->depthMode = eGL_NONE;
    if(IsDepthStencilFormat(details.internalFormat))
      gl.glGetTexParameteriv(details.curType, eGL_DEPTH_STENCIL_TEXTURE_MODE,
                                    (GLint *)&state->depthMode);

    gl.glGetTexParameteriv(details.curType, eGL_TEXTURE_BASE_LEVEL,
                                  (GLint *)&state->baseLevel);
    gl.glGetTexParameteriv(details.curType, eGL_TEXTURE_MAX_LEVEL,
                                  (GLint *)&state->maxLevel);
    gl.glGetTexParameteriv(details.curType, eGL_TEXTURE_SWIZZLE_R,
                                  (GLint *)&state->swizzle[0]);
    gl.glGetTexParameteriv(details.curType, eGL_TEXTURE_SWIZZLE_G,
                                  (GLint *)&state->swizzle[1]);
    gl.glGetTexParameteriv(details.curType, eGL_TEXTURE_SWIZZLE_B,
                                  (GLint *)&state->swizzle[2]);
    gl.glGetTexParameteriv(details.curType, eGL_TEXTURE_SWIZZLE_A,
                                  (GLint *)&state->swizzle[3]);

    // only non-ms textures have sampler state
    if(!ms)
    {
      gl.glGetTexParameteriv(details.curType, eGL_TEXTURE_SRGB_DECODE_EXT,
                                    (GLint *)&state->srgbDecode);
      gl.glGetTexParameteriv(details.curType, eGL_TEXTURE_COMPARE_FUNC,
                                    (GLint *)&state->compareFunc);
      gl.glGetTexParameteriv(details.curType, eGL_TEXTURE_COMPARE_MODE,
                                    (GLint *)&state->compareMode);
      gl.glGetTexParameteriv(details.curType, eGL_TEXTURE_MIN_FILTER,
                                    (GLint *)&state->minFilter);
      gl.glGetTexParameteriv(details.curType, eGL_TEXTURE_MAG_FILTER,
                                    (GLint *)&state->magFilter);
      gl.glGetTexParameteriv(details.curType, eGL_TEXTURE_WRAP_R,
                                    (GLint *)&state->wrap[0]);
      gl.glGetTexParameteriv(details.curType, eGL_TEXTURE_WRAP_S,
                                    (GLint *)&state->wrap[1]);
      gl.glGetTexParameteriv(details.curType, eGL_TEXTURE_WRAP_T,
                                    (GLint *)&state->wrap[2]);
      gl.glGetTexParameterfv(details.curType, eGL_TEXTURE_MIN_LOD, &state->minLod);
      gl.glGetTexParameterfv(details.curType, eGL_TEXTURE_MAX_LOD, &state->maxLod);
      gl.glGetTexParameterfv(details.curType, eGL_TEXTURE_BORDER_COLOR,
                                    &state->border[0]);
    }

    gl.glBindTexture(details.curType, oldtex);

    // we only copy contents for non-views
    GLuint tex = 0;

    if(!details.view)
    {
      {
        GLuint oldtex = 0;
        gl.glGetIntegerv(binding, (GLint *)&oldtex);

        gl.glGenTextures(1, &tex);
        gl.glBindTexture(details.curType, tex);

        gl.glBindTexture(details.curType, oldtex);
      }

      int depth = details.depth;
      if(details.curType != eGL_TEXTURE_3D)
        depth = 1;

      int mips =
          GetNumMips(gl, details.curType, res.name, details.width, details.height, details.depth);


      GLuint oldBinding;
      gl.glGetIntegerv(TextureBinding(details.curType), (GLint*)&oldBinding);
      gl.glBindTexture(details.curType, tex);

      // create texture of identical format/size to store initial contents
      if(details.curType == eGL_TEXTURE_2D_MULTISAMPLE)
      {
        gl.glTexStorage2DMultisample(details.curType, details.samples,
                                            details.internalFormat, details.width, details.height,
                                            GL_TRUE);
        mips = 1;
      }
      else if(details.curType == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
      {
        gl.glTexStorage3DMultisample(details.curType, details.samples,
                                            details.internalFormat, details.width, details.height,
                                            details.depth, GL_TRUE);
        mips = 1;
      }
      else if(details.dimension == 2)
      {
        gl.glTexStorage2D(details.curType, mips, details.internalFormat, details.width,
                                 details.height);
      }
      else if(details.dimension == 3)
      {
        gl.glTexStorage3D(details.curType, mips, details.internalFormat, details.width,
                                 details.height, details.depth);
      }

      // we need to set maxlevel appropriately for number of mips to force the texture to be
      // complete.
      // This can happen if e.g. a texture is initialised just by default with glTexImage for level
      // 0 and used as a framebuffer attachment, then the implementation is fine with it.
      // Unfortunately glCopyImageSubData requires completeness across all mips, a stricter
      // requirement :(.
      // We set max_level to mips - 1 (so mips=1 means MAX_LEVEL=0). Then restore it to the 'real'
      // value we fetched above
      int maxlevel = mips - 1;

      gl.glBindTexture(details.curType, res.name);
      gl.glTexParameteriv(details.curType, eGL_TEXTURE_MAX_LEVEL,
                                 (GLint *)&maxlevel);

      // copy over mips
      for(int i = 0; i < mips; i++)
      {
        int w = RDCMAX(details.width >> i, 1);
        int h = RDCMAX(details.height >> i, 1);
        int d = RDCMAX(details.depth >> i, 1);

        if(details.curType == eGL_TEXTURE_CUBE_MAP)
          d *= 6;
        else if(details.curType == eGL_TEXTURE_CUBE_MAP_ARRAY || details.curType == eGL_TEXTURE_2D_ARRAY)
          d = details.depth;

        // it seems like everything explodes if I do glCopyImageSubData on a D32F_S8 texture -
        // in-program the overlay gets corrupted as one UBO seems to not provide data anymore
        // until it's "refreshed". It seems like a driver bug, nvidia specific. In most cases a
        // program isn't going to rely on the contents of a depth-stencil buffer (shadow maps that
        // it might require would be depth-only formatted).
        if(details.internalFormat == eGL_DEPTH32F_STENCIL8 &&
           VendorCheck[VendorCheck_NV_avoid_D32S8_copy])
          RDCDEBUG("Not fetching initial contents of D32F_S8 texture");
        else
          gl.glCopyImageSubData(res.name, details.curType, i, 0, 0, 0, tex, details.curType, i, 0,
                                0, 0, w, h, d);
      }

      gl.glTexParameteriv(details.curType, eGL_TEXTURE_MAX_LEVEL,
                                 (GLint *)&state->maxLevel);
      gl.glBindTexture(details.curType, oldBinding);
    }

    SetInitialContents(origid, InitialContentData(TextureRes(res.Context, tex), 0, (byte *)state));
  }
  else
  {
    // record texbuffer only state
    GLuint oldBinding;
    gl.glGetIntegerv(TextureBinding(details.curType), (GLint*)&oldBinding);
    gl.glBindTexture(details.curType, res.name);

    GLuint bufName = 0;
    gl.glGetTexLevelParameteriv(details.curType, 0,
                                       eGL_TEXTURE_BUFFER_DATA_STORE_BINDING, (GLint *)&bufName);
    state->texBuffer = GetID(BufferRes(res.Context, bufName));

    gl.glGetTexLevelParameteriv(details.curType, 0, eGL_TEXTURE_BUFFER_OFFSET,
                                       (GLint *)&state->texBufOffs);
    gl.glGetTexLevelParameteriv(details.curType, 0, eGL_TEXTURE_BUFFER_SIZE,
                                       (GLint *)&state->texBufSize);

    SetInitialContents(origid, InitialContentData(GLResource(MakeNullResource), 0, (byte *)state));
    gl.glBindTexture(TextureBinding(details.curType), oldBinding);
  }
}

bool GLResourceManager::Force_InitialState(GLResource res, bool prepare)
{
  // TODO(elecro): check this.
  return false;
}

bool GLResourceManager::Serialise_InitialState(ResourceId resid, GLResource res)
{
  ResourceId Id = ResourceId();

  if(m_State >= WRITING)
  {
    Id = GetID(res);

    if(res.Namespace != eResBuffer)
      m_pSerialiser->Serialise("Id", Id);
  }
  else
  {
    m_pSerialiser->Serialise("Id", Id);
  }

  if(m_State < WRITING)
  {
    if(HasLiveResource(Id))
      res = GetLiveResource(Id);
    else
      res = GLResource(MakeNullResource);
  }

  const GLHookSet &gl = m_GL->m_Real;

  if(res.Namespace == eResBuffer)
  {
    // Nothing to serialize
  }
  else if(res.Namespace == eResProgram)
  {
    // Prepare_InitialState sets the serialise chunk directly on write,
    // so we should never come in here except for when reading
    RDCASSERT(m_State < WRITING);

//    WrappedGLES::ProgramData &details = m_GL->m_Programs[GetLiveID(Id)];
//
//    GLuint initProg = gl.glCreateProgram();
//
//    for(size_t i = 0; i < details.shaders.size(); i++)
//    {
//      const auto &shadDetails = m_GL->m_Shaders[details.shaders[i]];
//
//      GLuint shad = gl.glCreateShader(shadDetails.type);
//
//      char **srcs = new char *[shadDetails.sources.size()];
//      for(size_t s = 0; s < shadDetails.sources.size(); s++)
//        srcs[s] = (char *)shadDetails.sources[s].c_str();
//      gl.glShaderSource(shad, (GLsizei)shadDetails.sources.size(), srcs, NULL);
//
//      SAFE_DELETE_ARRAY(srcs);
//      gl.glCompileShader(shad);
//      gl.glAttachShader(initProg, shad);
//      gl.glDeleteShader(shad);
//    }
//
//    gl.glLinkProgram(initProg);
//
//    GLint status = 0;
//    gl.glGetProgramiv(initProg, eGL_LINK_STATUS, &status);
//
//    // if it failed to link, try again as a separable program.
//    // we can't do this by default because of the silly rules meaning
//    // shaders need fixup to be separable-compatible.
//    if(status == 0)
//    {
//      gl.glProgramParameteri(initProg, eGL_PROGRAM_SEPARABLE, 1);
//      gl.glLinkProgram(initProg);
//
//      gl.glGetProgramiv(initProg, eGL_LINK_STATUS, &status);
//    }
//
//    if(status == 0)
//    {
//      if(details.shaders.size() == 0)
//      {
//        RDCWARN("No shaders attached to program");
//      }
//      else
//      {
//        char buffer[1025] = {0};
//        gl.glGetProgramInfoLog(initProg, 1024, NULL, buffer);
//        RDCERR("Link error: %s", buffer);
//      }
//    }
//
//    SerialiseProgramUniforms(gl, m_pSerialiser, initProg, &details.locationTranslate, false);
//
//    SetInitialContents(Id, InitialContentData(ProgramRes(m_GL->GetCtx(), initProg), 0, NULL));

    // TODO PEPE: Due to the TFBO varying bindings are missing from the code above we reuse the already
    // linked program instead of crating a new one as the location queries can be wrong without them.`
    WrappedGLES::ProgramData &details = m_GL->m_Programs[GetLiveID(Id)];
    GLuint initProg = GetLiveResource(Id).name;
    SerialiseProgramUniforms(gl, m_pSerialiser, initProg, &details.locationTranslate, false);

    SetInitialContents(Id, InitialContentData(ProgramRes(m_GL->GetCtx(), initProg), 0, NULL));
  }
  else if(res.Namespace == eResTexture)
  {
    if(m_State >= WRITING)
    {
      WrappedGLES::TextureData &details = m_GL->m_Textures[Id];

      SERIALISE_ELEMENT(GLenum, f, details.internalFormat);

      // only continue with the rest if the format is valid (storage allocated)
      if(f != eGL_NONE)
      {
        GLuint tex = GetInitialContents(Id).resource.name;

        GLuint ppb = 0;
        gl.glGetIntegerv(eGL_PIXEL_PACK_BUFFER_BINDING, (GLint *)&ppb);
        gl.glBindBuffer(eGL_PIXEL_PACK_BUFFER, 0);

        GLint packParams[8];
        gl.glGetIntegerv(eGL_PACK_ROW_LENGTH, &packParams[2]);
        gl.glGetIntegerv(eGL_PACK_SKIP_PIXELS, &packParams[4]);
        gl.glGetIntegerv(eGL_PACK_SKIP_ROWS, &packParams[5]);
        gl.glGetIntegerv(eGL_PACK_ALIGNMENT, &packParams[7]);

        gl.glPixelStorei(eGL_PACK_ROW_LENGTH, 0);
        gl.glPixelStorei(eGL_PACK_SKIP_PIXELS, 0);
        gl.glPixelStorei(eGL_PACK_SKIP_ROWS, 0);
        gl.glPixelStorei(eGL_PACK_ALIGNMENT, 1);

        int imgmips = 1;

        if(details.curType != eGL_TEXTURE_BUFFER)
          imgmips =
              GetNumMips(gl, details.curType, tex, details.width, details.height, details.depth);

        TextureStateInitialData *state = (TextureStateInitialData *)GetInitialContents(Id).blob;

        SERIALISE_ELEMENT(TextureStateInitialData, stateData, *state);

        SERIALISE_ELEMENT(uint32_t, width, details.width);
        SERIALISE_ELEMENT(uint32_t, height, details.height);
        SERIALISE_ELEMENT(uint32_t, depth, details.depth);
        SERIALISE_ELEMENT(uint32_t, samples, details.samples);
        SERIALISE_ELEMENT(uint32_t, dim, details.dimension);
        SERIALISE_ELEMENT(GLenum, t, details.curType);
        SERIALISE_ELEMENT(int, mips, imgmips);

        SERIALISE_ELEMENT(bool, isCompressed, IsCompressedFormat(details.internalFormat));

        if(details.curType == eGL_TEXTURE_BUFFER || details.view)
        {
          // no contents to copy for texture buffer (it's copied under the buffer)
          // same applies for texture views, their data is copies under the aliased texture
        }
        else if(isCompressed)
        {
          for(int i = 0; i < mips; i++)
          {
            GLenum targets[] = {
                eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
            };

            int count = ARRAY_COUNT(targets);

            if(t != eGL_TEXTURE_CUBE_MAP)
            {
              targets[0] = details.curType;
              count = 1;
            }

            for(int trg = 0; trg < count; trg++)
            {
              size_t size = details.compressedData[targets[trg]][i].size();
              byte *buf = new byte[size];
              memcpy(buf, details.compressedData[targets[trg]][i].data(), size);
              m_pSerialiser->SerialiseBuffer("image", buf, size);
              delete[] buf;
            }
          }
        }
        else if(samples > 1)
        {
          GLNOTIMP("Not implemented - initial states of multisampled textures");
        }
        else
        {
          GLenum fmt = GetBaseFormat(details.internalFormat);
          GLenum type = GetDataType(details.internalFormat);

          size_t size = GetByteSize(details.width, details.height, details.depth, fmt, type);

          byte *buf = new byte[size];

          GLenum binding = TextureBinding(t);

          GLuint prevtex = 0;
          gl.glGetIntegerv(binding, (GLint *)&prevtex);

          gl.glBindTexture(t, tex);

          for(int i = 0; i < mips; i++)
          {
            int w = RDCMAX(details.width >> i, 1);
            int h = RDCMAX(details.height >> i, 1);
            int d = RDCMAX(details.depth >> i, 1);

            if(t == eGL_TEXTURE_CUBE_MAP_ARRAY || t == eGL_TEXTURE_2D_ARRAY)
              d = details.depth;

            size = GetByteSize(w, h, d, fmt, type);

            GLenum targets[] = {
                eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
            };

            int count = ARRAY_COUNT(targets);

            if(t != eGL_TEXTURE_CUBE_MAP)
            {
              targets[0] = t;
              count = 1;
            }

            for(int trg = 0; trg < count; trg++)
            {
              // OpenGL version: gl.glGetTexImage(targets[trg], i, fmt, type, buf)
              m_GL->Compat_glGetTexImage(targets[trg], t, tex, i, fmt, type, w, h, d, buf);
              m_pSerialiser->SerialiseBuffer("image", buf, size);
            }
          }

          gl.glBindTexture(t, prevtex);

          SAFE_DELETE_ARRAY(buf);
        }

        gl.glBindBuffer(eGL_PIXEL_PACK_BUFFER, ppb);

        gl.glPixelStorei(eGL_PACK_ROW_LENGTH, packParams[2]);
        gl.glPixelStorei(eGL_PACK_SKIP_PIXELS, packParams[4]);
        gl.glPixelStorei(eGL_PACK_SKIP_ROWS, packParams[5]);
        gl.glPixelStorei(eGL_PACK_ALIGNMENT, packParams[7]);
      }
    }
    else
    {
      WrappedGLES::TextureData &details = m_GL->m_Textures[GetLiveID(Id)];

      SERIALISE_ELEMENT(GLenum, internalformat, eGL_NONE);

      if(internalformat != eGL_NONE)
      {
        GLuint pub = 0;
        gl.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, (GLint *)&pub);
        gl.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

        GLint unpackParams[8];
        gl.glGetIntegerv(eGL_UNPACK_ROW_LENGTH, &unpackParams[2]);
        gl.glGetIntegerv(eGL_UNPACK_SKIP_PIXELS, &unpackParams[4]);
        gl.glGetIntegerv(eGL_UNPACK_SKIP_ROWS, &unpackParams[5]);
        gl.glGetIntegerv(eGL_UNPACK_ALIGNMENT, &unpackParams[7]);

        gl.glPixelStorei(eGL_UNPACK_ROW_LENGTH, 0);
        gl.glPixelStorei(eGL_UNPACK_SKIP_PIXELS, 0);
        gl.glPixelStorei(eGL_UNPACK_SKIP_ROWS, 0);
        gl.glPixelStorei(eGL_UNPACK_ALIGNMENT, 1);

        TextureStateInitialData *state = (TextureStateInitialData *)Serialiser::AllocAlignedBuffer(
            sizeof(TextureStateInitialData));
        RDCEraseMem(state, sizeof(TextureStateInitialData));

        m_pSerialiser->Serialise("state", *state);

        SERIALISE_ELEMENT(uint32_t, width, 0);
        SERIALISE_ELEMENT(uint32_t, height, 0);
        SERIALISE_ELEMENT(uint32_t, depth, 0);
        SERIALISE_ELEMENT(uint32_t, samples, 0);
        SERIALISE_ELEMENT(uint32_t, dim, 0);
        SERIALISE_ELEMENT(GLenum, textype, eGL_NONE);
        SERIALISE_ELEMENT(int, mips, 0);
        SERIALISE_ELEMENT(bool, isCompressed, false);

        // if number of mips isn't sufficient, make sure to initialise
        // the lower levels - this could happen if e.g. a texture is
        // init'd with glTexImage(level = 0), then after we stop tracking
        // it glGenerateMipmap is called

        GLuint live = GetLiveResource(Id).name;

        // this is only relevant for non-immutable textures though
        GLint immut = 0;

        if(textype != eGL_TEXTURE_BUFFER) {
          GLuint oldBinding;
          gl.glGetIntegerv(TextureBinding(textype), (GLint*)&oldBinding);
          gl.glBindTexture(textype, live);
          gl.glGetTexParameteriv(textype, eGL_TEXTURE_IMMUTABLE_FORMAT, &immut);
          gl.glBindTexture(textype, oldBinding);
        }

        if(textype != eGL_TEXTURE_BUFFER && immut == 0)
        {
          GLsizei w = (GLsizei)width;
          GLsizei h = (GLsizei)height;
          GLsizei d = (GLsizei)depth;

          int liveMips = GetNumMips(gl, textype, live, width, height, depth);

          GLenum targets[] = {
              eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
              eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
              eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
          };

          int count = ARRAY_COUNT(targets);

          if(textype != eGL_TEXTURE_CUBE_MAP)
          {
            targets[0] = textype;
            count = 1;
          }

          for(int m = 1; m < mips; m++)
          {
            w = RDCMAX(1, w >> 1);
            h = RDCMAX(1, h >> 1);
            d = RDCMAX(1, d >> 1);

            if(textype == eGL_TEXTURE_CUBE_MAP_ARRAY || textype == eGL_TEXTURE_2D_ARRAY)
              d = (GLsizei)depth;

            if(m >= liveMips)
            {
              for(int t = 0; t < count; t++)
              {
                GLuint oldBinding;
                gl.glGetIntegerv(TextureBinding(textype), (GLint*)&oldBinding);
                gl.glBindTexture(textype, live);
                if(isCompressed)
                {
                  GLsizei compSize = (GLsizei)GetCompressedByteSize(w, h, d, internalformat, m);

                  vector<byte> dummy;
                  dummy.resize(compSize);

                  if(dim == 2)
                    gl.glCompressedTexImage2D(targets[t], m, internalformat, w, h, 0,
                                                     compSize, &dummy[0]);
                  else if(dim == 3)
                    gl.glCompressedTexImage3D(targets[t], m, internalformat, w, h, d,
                                                     0, compSize, &dummy[0]);
                }
                else
                {
                  if(dim == 2)
                    gl.glTexImage2D(targets[t], m, internalformat, (GLsizei)w,
                                           (GLsizei)h, 0, GetBaseFormat(internalformat),
                                           GetDataType(internalformat), NULL);
                  else if(dim == 3)
                    gl.glTexImage3D(targets[t], m, internalformat, (GLsizei)w,
                                           (GLsizei)h, (GLsizei)d, 0, GetBaseFormat(internalformat),
                                           GetDataType(internalformat), NULL);
                }
                gl.glBindTexture(textype, oldBinding);
              }
            }
          }
        }

        GLuint tex = 0;

        if(textype != eGL_TEXTURE_BUFFER && !details.view)
        {
          gl.glGenTextures(1, &tex);
        }

        GLenum dummy;
        EmulateLuminanceFormat(gl, tex, textype, internalformat, dummy);
        GLuint oldBinding = 0;

        // create texture of identical format/size to store initial contents
        if(textype == eGL_TEXTURE_BUFFER || details.view)
        {
          // no 'contents' texture to create
        }
        else if(textype == eGL_TEXTURE_2D_MULTISAMPLE)
        {
          gl.glGetIntegerv(TextureBinding(textype), (GLint*)&oldBinding);
          gl.glBindTexture(textype, tex);
          gl.glTexStorage2DMultisample(textype, samples, internalformat, width, height,
                                              GL_TRUE);
          gl.glBindTexture(textype, oldBinding);
          mips = 1;
        }
        else if(textype == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
        {
          gl.glGetIntegerv(TextureBinding(textype), (GLint*)&oldBinding);
          gl.glBindTexture(textype, tex);
          gl.glTexStorage3DMultisample(textype, samples, internalformat, width, height,
                                              depth, GL_TRUE);
          gl.glBindTexture(textype, oldBinding);
          mips = 1;
        }
        else if(dim == 2)
        {
          gl.glGetIntegerv(TextureBinding(textype), (GLint*)&oldBinding);
          gl.glBindTexture(textype, tex);
          gl.glTexStorage2D(textype, mips, internalformat, width, height);
          gl.glBindTexture(textype, oldBinding);
        }
        else if(dim == 3)
        {
          gl.glGetIntegerv(TextureBinding(textype), (GLint*)&oldBinding);
          gl.glBindTexture(textype, tex);
          gl.glTexStorage3D(textype, mips, internalformat, width, height, depth);
          gl.glBindTexture(textype, oldBinding);
        }

        if(textype == eGL_TEXTURE_BUFFER || details.view)
        {
          // no contents to serialise
        }
        else if(isCompressed)
        {
          for(int i = 0; i < mips; i++)
          {
            uint32_t w = RDCMAX(width >> i, 1U);
            uint32_t h = RDCMAX(height >> i, 1U);
            uint32_t d = RDCMAX(depth >> i, 1U);

            if(textype == eGL_TEXTURE_CUBE_MAP_ARRAY || textype == eGL_TEXTURE_2D_ARRAY)
              d = depth;

            GLenum targets[] = {
                eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
            };

            int count = ARRAY_COUNT(targets);

            if(textype != eGL_TEXTURE_CUBE_MAP)
            {
              targets[0] = textype;
              count = 1;
            }

            gl.glGetIntegerv(TextureBinding(textype), (GLint*)&oldBinding);
            gl.glBindTexture(textype, tex);

            for(int trg = 0; trg < count; trg++)
            {
              size_t size = 0;
              byte *buf = NULL;

              m_pSerialiser->SerialiseBuffer("image", buf, size);

              size_t compSize = GetCompressedByteSize(w, h, d, internalformat, i);
              if (size != compSize)
              {
                RDCWARN("Loaded compressed image size (%d) differs from the expected size (%d)!", size, compSize);
                buf = (byte*)realloc(buf, compSize);
                RDCEraseMem(buf, compSize);
              }

              if(dim == 2)
                gl.glCompressedTexSubImage2D(targets[trg], i, 0, 0, w, h,
                                                    internalformat, (GLsizei)compSize, buf);
              else if(dim == 3)
                gl.glCompressedTexSubImage3D(targets[trg], i, 0, 0, 0, w, h, d,
                                                    internalformat, (GLsizei)compSize, buf);
              SAFE_DELETE(buf);
            }

            gl.glBindTexture(textype, oldBinding);

          }
        }
        else if(samples > 1)
        {
          GLNOTIMP("Not implemented - initial states of multisampled textures");
        }
        else
        {
          GLenum fmt = GetBaseFormat(internalformat);
          GLenum type = GetDataType(internalformat);

          for(int i = 0; i < mips; i++)
          {
            uint32_t w = RDCMAX(width >> i, 1U);
            uint32_t h = RDCMAX(height >> i, 1U);
            uint32_t d = RDCMAX(depth >> i, 1U);

            if(textype == eGL_TEXTURE_CUBE_MAP_ARRAY || textype == eGL_TEXTURE_2D_ARRAY)
              d = depth;

            GLenum targets[] = {
                eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
            };

            int count = ARRAY_COUNT(targets);

            if(textype != eGL_TEXTURE_CUBE_MAP)
            {
              targets[0] = textype;
              count = 1;
            }

            gl.glGetIntegerv(TextureBinding(textype), (GLint*)&oldBinding);
            gl.glBindTexture(textype, tex);

            for(int trg = 0; trg < count; trg++)
            {
              size_t size = 0;
              byte *buf = NULL;
              m_pSerialiser->SerialiseBuffer("image", buf, size);

              if(dim == 2)
                gl.glTexSubImage2D(targets[trg], i, 0, 0, w, h, fmt, type, buf);
              else if(dim == 3)
                gl.glTexSubImage3D(targets[trg], i, 0, 0, 0, w, h, d, fmt, type, buf);

              SAFE_DELETE(buf);
            }

            gl.glBindTexture(textype, oldBinding);
          }
        }

        if(textype != eGL_TEXTURE_BUFFER && !details.view)
          SetInitialContents(Id,
                             InitialContentData(TextureRes(m_GL->GetCtx(), tex), 0, (byte *)state));
        else
          SetInitialContents(Id, InitialContentData(GLResource(MakeNullResource), 0, (byte *)state));

        gl.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, pub);

        gl.glPixelStorei(eGL_UNPACK_ROW_LENGTH, unpackParams[2]);
        gl.glPixelStorei(eGL_UNPACK_SKIP_PIXELS, unpackParams[4]);
        gl.glPixelStorei(eGL_UNPACK_SKIP_ROWS, unpackParams[5]);
        gl.glPixelStorei(eGL_UNPACK_ALIGNMENT, unpackParams[7]);
      }
    }
  }
  else if(res.Namespace == eResFramebuffer)
  {
    FramebufferInitialData data;

    if(m_State >= WRITING)
    {
      FramebufferInitialData *initialdata = (FramebufferInitialData *)GetInitialContents(Id).blob;
      memcpy(&data, initialdata, sizeof(data));
    }

    m_pSerialiser->Serialise("Framebuffer object Buffers", data);

    if(m_State < WRITING)
    {
      byte *blob = Serialiser::AllocAlignedBuffer(sizeof(data));
      memcpy(blob, &data, sizeof(data));

      SetInitialContents(Id, InitialContentData(GLResource(MakeNullResource), 0, blob));
    }
  }
  else if(res.Namespace == eResFeedback)
  {
    FeedbackInitialData data;

    if(m_State >= WRITING)
    {
      FeedbackInitialData *initialdata = (FeedbackInitialData *)GetInitialContents(Id).blob;
      memcpy(&data, initialdata, sizeof(data));
    }

    m_pSerialiser->Serialise("Transform Feedback Buffers", data);

    if(m_State < WRITING)
    {
      byte *blob = Serialiser::AllocAlignedBuffer(sizeof(data));
      memcpy(blob, &data, sizeof(data));

      SetInitialContents(Id, InitialContentData(GLResource(MakeNullResource), 0, blob));
    }
  }
  else if(res.Namespace == eResVertexArray)
  {
    VAOInitialData data;

    if(m_State >= WRITING)
    {
      VAOInitialData *initialdata = (VAOInitialData *)GetInitialContents(Id).blob;
      memcpy(&data, initialdata, sizeof(data));
    }

    m_pSerialiser->Serialise("valid", data.valid);
    for(GLuint i = 0; i < 16; i++)
    {
      m_pSerialiser->Serialise("VertexAttrib[]", data.VertexAttribs[i]);
      m_pSerialiser->Serialise("VertexBuffer[]", data.VertexBuffers[i]);
    }
    m_pSerialiser->Serialise("ElementArrayBuffer", data.ElementArrayBuffer);

    if(m_State < WRITING)
    {
      byte *blob = Serialiser::AllocAlignedBuffer(sizeof(data));
      memcpy(blob, &data, sizeof(data));

      SetInitialContents(Id, InitialContentData(GLResource(MakeNullResource), 0, blob));
    }
  }
  else if(res.Namespace == eResRenderbuffer)
  {
    RDCWARN(
        "Technically you could try and readback the contents of a RenderBuffer via pixel copy.");
    RDCWARN("Currently we don't support that though, and initial contents will be uninitialised.");
  }
  else
  {
    RDCERR("Unexpected type of resource requiring initial state");
  }

  return true;
}

void GLResourceManager::Create_InitialState(ResourceId id, GLResource live, bool hasData)
{
  if(live.Namespace == eResTexture)
  {
    // we basically need to do exactly the same as Prepare_InitialState -
    // save current texture state, create a duplicate object, and save
    // the current contents into that duplicate object

    // in future if we skip RT contents for write-before-read RTs, we could mark
    // textures to be cleared instead of copied.
    PrepareTextureInitialContents(GetID(live), id, live);
  }
  else if(live.Namespace == eResVertexArray)
  {
    byte *data = Serialiser::AllocAlignedBuffer(sizeof(VAOInitialData));
    RDCEraseMem(data, sizeof(VAOInitialData));

    SetInitialContents(id, InitialContentData(GLResource(MakeNullResource), 0, data));

    Prepare_InitialState(live, data);
  }
  else if(live.Namespace != eResBuffer && live.Namespace != eResProgram &&
          live.Namespace != eResRenderbuffer)
  {
    RDCUNIMPLEMENTED("Expect all initial states to be created & not skipped, presently");
  }
}

void GLResourceManager::Apply_InitialState(GLResource live, InitialContentData initial)
{
  const GLHookSet &gl = m_GL->m_Real;

  if(live.Namespace == eResTexture)
  {
    ResourceId Id = GetID(live);
    WrappedGLES::TextureData &details = m_GL->m_Textures[Id];

    TextureStateInitialData *state = (TextureStateInitialData *)initial.blob;

    if(details.curType != eGL_TEXTURE_BUFFER)
    {
      GLuint tex = initial.resource.name;

      if(initial.resource != GLResource(MakeNullResource) && tex != 0)
      {
        int mips = GetNumMips(gl, details.curType, tex, details.width, details.height, details.depth);

        // we need to set maxlevel appropriately for number of mips to force the texture to be
        // complete. This can happen if e.g. a texture is initialised just by default with
        // glTexImage for level 0 and used as a framebuffer attachment, then the implementation is
        // fine with it.
        // Unfortunately glCopyImageSubData requires completeness across all mips, a stricter
        // requirement :(.
        // We set max_level to mips - 1 (so mips=1 means MAX_LEVEL=0). Then below where we set the
        // texture state, the correct MAX_LEVEL is set to whatever the program had.
        int maxlevel = mips - 1;

        GLuint oldBinding;
        gl.glGetIntegerv(TextureBinding(details.curType), (GLint*)&oldBinding);
        gl.glBindTexture(details.curType, live.name);
        gl.glTexParameteriv(details.curType, eGL_TEXTURE_MAX_LEVEL,
                                   (GLint *)&maxlevel);
        gl.glBindTexture(details.curType, oldBinding);

        // copy over mips
        for(int i = 0; i < mips; i++)
        {
          int w = RDCMAX(details.width >> i, 1);
          int h = RDCMAX(details.height >> i, 1);
          int d = RDCMAX(details.depth >> i, 1);

          if(details.curType == eGL_TEXTURE_CUBE_MAP)
            d *= 6;
          else if(details.curType == eGL_TEXTURE_CUBE_MAP_ARRAY || details.curType == eGL_TEXTURE_2D_ARRAY)
            d = details.depth;

          // it seems like everything explodes if I do glCopyImageSubData on a D32F_S8 texture -
          // on replay loads of things get heavily corrupted - probably the same as the problems
          // we get in-program, but magnified. It seems like a driver bug, nvidia specific.
          // In most cases a program isn't going to rely on the contents of a depth-stencil buffer
          // (shadow maps that it might require would be depth-only formatted).
          if(details.internalFormat == eGL_DEPTH32F_STENCIL8 &&
             VendorCheck[VendorCheck_NV_avoid_D32S8_copy])
            RDCDEBUG("Not fetching initial contents of D32F_S8 texture");
          else
            gl.glCopyImageSubData(tex, details.curType, i, 0, 0, 0, live.name, details.curType, i,
                                  0, 0, 0, w, h, d);
        }

      }

      bool ms = (details.curType == eGL_TEXTURE_2D_MULTISAMPLE ||
                 details.curType == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY);

      GLuint oldBinding;
      gl.glGetIntegerv(TextureBinding(details.curType), (GLint*)&oldBinding);
      gl.glBindTexture(details.curType, live.name);

      if(state->depthMode == eGL_DEPTH_COMPONENT || state->depthMode == eGL_STENCIL_INDEX)
        gl.glTexParameteriv(details.curType, eGL_DEPTH_STENCIL_TEXTURE_MODE,
                                   (GLint *)&state->depthMode);

      gl.glTexParameteriv(details.curType, eGL_TEXTURE_BASE_LEVEL,
                                 (GLint *)&state->baseLevel);
      gl.glTexParameteriv(details.curType, eGL_TEXTURE_MAX_LEVEL,
                                 (GLint *)&state->maxLevel);

      // assume that emulated (luminance, alpha-only etc) textures are not swizzled
      if(!details.emulated) {
        gl.glTexParameteriv(details.curType, eGL_TEXTURE_SWIZZLE_R,
                                   (GLint *)&state->swizzle[0]);
        gl.glTexParameteriv(details.curType, eGL_TEXTURE_SWIZZLE_G,
                                   (GLint *)&state->swizzle[1]);
        gl.glTexParameteriv(details.curType, eGL_TEXTURE_SWIZZLE_B,
                                   (GLint *)&state->swizzle[2]);
        gl.glTexParameteriv(details.curType, eGL_TEXTURE_SWIZZLE_A,
                                   (GLint *)&state->swizzle[3]);
      }

      if(!ms)
      {
        gl.glTexParameteriv(details.curType, eGL_TEXTURE_SRGB_DECODE_EXT,
                                   (GLint *)&state->srgbDecode);
        gl.glTexParameteriv(details.curType, eGL_TEXTURE_COMPARE_FUNC,
                                   (GLint *)&state->compareFunc);
        gl.glTexParameteriv(details.curType, eGL_TEXTURE_COMPARE_MODE,
                                   (GLint *)&state->compareMode);
        gl.glTexParameteriv(details.curType, eGL_TEXTURE_MIN_FILTER,
                                   (GLint *)&state->minFilter);
        gl.glTexParameteriv(details.curType, eGL_TEXTURE_MAG_FILTER,
                                   (GLint *)&state->magFilter);
        gl.glTexParameteriv(details.curType, eGL_TEXTURE_WRAP_R,
                                   (GLint *)&state->wrap[0]);
        gl.glTexParameteriv(details.curType, eGL_TEXTURE_WRAP_S,
                                   (GLint *)&state->wrap[1]);
        gl.glTexParameteriv(details.curType, eGL_TEXTURE_WRAP_T,
                                   (GLint *)&state->wrap[2]);
        gl.glTexParameterfv(details.curType, eGL_TEXTURE_BORDER_COLOR,
                                   state->border);
      }
      gl.glBindTexture(details.curType, oldBinding);
    }
    else
    {
      GLuint buffer = 0;

      if(HasLiveResource(state->texBuffer))
        buffer = GetLiveResource(state->texBuffer).name;

      GLenum fmt = details.internalFormat;

      // update width from here as it's authoratitive - the texture might have been resized in
      // multiple rebinds that we will not have serialised before.
      details.width =
          state->texBufSize / uint32_t(GetByteSize(1, 1, 1, GetBaseFormat(fmt), GetDataType(fmt)));

      if(gl.glTexBufferRange)
      {
        // restore texbuffer only state
        GLuint oldBinding;
        gl.glGetIntegerv(eGL_TEXTURE_BUFFER_BINDING, (GLint*)&oldBinding);
        gl.glBindBuffer(eGL_TEXTURE_BUFFER, buffer);
        gl.glTexBufferRange(eGL_TEXTURE_BUFFER, details.internalFormat, buffer,
                                   state->texBufOffs, state->texBufSize);
        gl.glBindBuffer(eGL_TEXTURE_BUFFER, oldBinding);
      }
      else
      {
        GLuint oldBinding;
        gl.glGetIntegerv(eGL_TEXTURE_BUFFER_BINDING, (GLint*)&oldBinding);
        gl.glBindBuffer(eGL_TEXTURE_BUFFER, buffer);

        uint32_t bufSize = 0;
        gl.glGetBufferParameteriv(eGL_TEXTURE_BUFFER, eGL_BUFFER_SIZE, (GLint *)&bufSize);
        if(state->texBufOffs > 0 || state->texBufSize > bufSize)
        {
          const char *msg =
              "glTextureBufferRangeEXT is not supported on your GL implementation, but is needed "
              "for correct replay.\n"
              "The original capture created a texture buffer with a range - replay will use the "
              "whole buffer, which is likely incorrect.";
          RDCERR("%s", msg);
          m_GL->AddDebugMessage(eDbgCategory_Resource_Manipulation, eDbgSeverity_High,
                                eDbgSource_IncorrectAPIUse, msg);
        }

        gl.glTexBuffer(eGL_TEXTURE_BUFFER, details.internalFormat, buffer);
        gl.glBindBuffer(eGL_TEXTURE_BUFFER, oldBinding);

      }
    }
  }
  else if(live.Namespace == eResProgram)
  {
    CopyProgramUniforms(gl, initial.resource.name, live.name);
  }
  else if(live.Namespace == eResFramebuffer)
  {
    FramebufferInitialData *data = (FramebufferInitialData *)initial.blob;

    if(data->valid)
    {
      GLuint prevread = 0, prevdraw = 0;
      gl.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&prevdraw);
      gl.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&prevread);

      gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, live.name);
      gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, live.name);

      for(int i = 0; i < (int)ARRAY_COUNT(data->Attachments); i++)
      {
        FramebufferAttachmentData &a = data->Attachments[i];

        GLuint obj = a.obj == ResourceId() ? 0 : GetLiveResource(a.obj).name;

        if(a.renderbuffer && obj)
        {
          gl.glFramebufferRenderbuffer(eGL_FRAMEBUFFER, data->attachmentNames[i],
                                               eGL_RENDERBUFFER, obj);
        }
        else
        {
          if(!a.layered && obj)
          {
            // we use old-style non-DSA for this because binding cubemap faces with EXT_dsa
            // is completely messed up and broken

            // if obj is a cubemap use face-specific targets
            WrappedGLES::TextureData &details = m_GL->m_Textures[GetLiveID(a.obj)];

            if(details.curType == eGL_TEXTURE_CUBE_MAP)
            {
              GLenum faces[] = {
                  eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                  eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                  eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
              };

              if(a.layer < 6)
              {
                gl.glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, data->attachmentNames[i],
                                          faces[a.layer], obj, a.level);
              }
              else
              {
                RDCWARN("Invalid layer %u used to bind cubemap to framebuffer. Binding POSITIVE_X");
                gl.glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, data->attachmentNames[i], faces[0],
                                          obj, a.level);
              }
            }
            else if(details.curType == eGL_TEXTURE_CUBE_MAP_ARRAY ||
                    details.curType == eGL_TEXTURE_2D_ARRAY)
            {
              gl.glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, data->attachmentNames[i], obj,
                                           a.level, a.layer);
            }
            else
            {
              RDCASSERT(a.layer == 0);
              gl.glFramebufferTexture(eGL_FRAMEBUFFER, data->attachmentNames[i], obj, a.level);
            }
          }
          else
          {
            gl.glFramebufferTexture(eGL_FRAMEBUFFER, data->attachmentNames[i], obj, a.level);
          }
        }
      }

      // set invalid caps to GL_COLOR_ATTACHMENT0
      for(int i = 0; i < (int)ARRAY_COUNT(data->DrawBuffers); i++)
        if(data->DrawBuffers[i] == eGL_BACK || data->DrawBuffers[i] == eGL_FRONT)
          data->DrawBuffers[i] = eGL_COLOR_ATTACHMENT0;
      if(data->ReadBuffer == eGL_BACK || data->ReadBuffer == eGL_FRONT)
        data->ReadBuffer = eGL_COLOR_ATTACHMENT0;

      gl.glDrawBuffers(ARRAY_COUNT(data->DrawBuffers), data->DrawBuffers);

      gl.glReadBuffer(data->ReadBuffer);

      gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, prevdraw);
      gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, prevread);
    }
  }
  else if(live.Namespace == eResFeedback)
  {
    FeedbackInitialData *data = (FeedbackInitialData *)initial.blob;

    if(data->valid)
    {
      GLuint prevfeedback = 0;
      gl.glGetIntegerv(eGL_TRANSFORM_FEEDBACK_BINDING, (GLint *)&prevfeedback);

      gl.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, live.name);

      GLint maxCount = 0;
      gl.glGetIntegerv(eGL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &maxCount);

      for(int i = 0; i < (int)ARRAY_COUNT(data->Buffer) && i < maxCount; i++)
      {
        GLuint buffer = data->Buffer[i] == ResourceId() ? 0 : GetLiveResource(data->Buffer[i]).name;
        if(buffer == 0 || ((GLintptr)data->Offset[i] == 0 && (GLsizei)data->Size[i] == 0))
          gl.glBindBufferBase(eGL_TRANSFORM_FEEDBACK_BUFFER, i, buffer);
        else
          gl.glBindBufferRange(eGL_TRANSFORM_FEEDBACK_BUFFER, i, buffer, (GLintptr)data->Offset[i],
                               (GLsizei)data->Size[i]);
      }

      gl.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, prevfeedback);
    }
  }
  else if(live.Namespace == eResVertexArray)
  {
    VAOInitialData *initialdata = (VAOInitialData *)initial.blob;

    if(initialdata->valid)
    {
      GLuint VAO = 0;
      gl.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&VAO);

      if(live.name == 0)
        gl.glBindVertexArray(m_GL->GetFakeVAO());
      else
        gl.glBindVertexArray(live.name);

      SafeBufferBinder safeBufferBinder(gl, eGL_ARRAY_BUFFER, 0);

      for(GLuint i = 0; i < 16; i++)
      {
        VertexAttribInitialData &attrib = initialdata->VertexAttribs[i];

        if(attrib.enabled)
          gl.glEnableVertexAttribArray(i);
        else
          gl.glDisableVertexAttribArray(i);

        if (live.name != 0)
        {
          gl.glVertexAttribBinding(i, attrib.vbslot);

          if(attrib.size != 0)
          {
            if(initialdata->VertexAttribs[i].integer == 0)
              gl.glVertexAttribFormat(i, attrib.size, attrib.type, (GLboolean)attrib.normalized,
                                      attrib.offset);
            else
              gl.glVertexAttribIFormat(i, attrib.size, attrib.type, attrib.offset);
          }

          VertexBufferInitialData &buf = initialdata->VertexBuffers[i];
          GLuint buffer = buf.Buffer == ResourceId() ? 0 : GetLiveResource(buf.Buffer).name;
          gl.glBindVertexBuffer(i, buffer, (GLintptr)buf.Offset, (GLsizei)buf.Stride);
          gl.glVertexBindingDivisor(i, buf.Divisor);
        }
        else
        {
          VertexBufferInitialData &buf = initialdata->VertexBuffers[i];
          GLuint buffer = buf.Buffer == ResourceId() ? 0 : GetLiveResource(buf.Buffer).name;
          gl.glBindBuffer(eGL_ARRAY_BUFFER, buffer);

          if(initialdata->VertexAttribs[i].integer == 0)
            gl.glVertexAttribPointer(i, attrib.size, attrib.type, (GLboolean)attrib.normalized,
                                     buf.Stride, (const GLvoid *)buf.Offset);
          else
            gl.glVertexAttribIPointer(i, attrib.size, attrib.type,
                                      buf.Stride, (const GLvoid *)buf.Offset);

          gl.glVertexAttribDivisor(i, buf.Divisor);
        }
      }

      GLuint buffer = initialdata->ElementArrayBuffer == ResourceId()
                          ? 0
                          : GetLiveResource(initialdata->ElementArrayBuffer).name;
      gl.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, buffer);

      gl.glBindVertexArray(VAO);
    }
  }
  else if(live.Namespace == eResRenderbuffer)
  {
  }
  else
  {
    RDCERR("Unexpected type of resource requiring initial state");
  }
}
